"""Benchmark Y-Player (and optionally PotPlayer) on the corpus; one command reruns everything.

    python bench/run_bench.py --label baseline [--repeats 3] [--players yplayer,potplayer]
                              [--clips id,id] [--extra PATH ...] [--sample 8] [--warmup 4]

For every (repeat, clip, player) the runner launches the player, waits for its main window,
gives every player the same window size, waits `warmup` seconds, then samples for `sample`
seconds from *outside* the process, identically for all players:
  CPU (process time / wall time), working set, private bytes, GPU engine utilisation per engine
  type (3D, VideoDecode, VideoProcessing, Copy) and GPU memory, via Windows PDH counters.
Y-Player additionally runs with --bench-report and reports what only it can see (startup,
first frame, dropped/delayed frames, A/V sync, seek latency). Idle runs (no file) measure
the resting cost. The default audio device is muted for the whole run and restored after.

Results: bench/results/<date>-<label>.json (raw + summary) and .md (tables).
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import json
import platform
import statistics
import subprocess
import sys
import tempfile
import time
import winreg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "bench"
YPLAYER = ROOT / "build" / "release" / "bin" / "Y-Player.exe"
POTPLAYER = Path(r"C:\Program Files\DAUM\PotPlayer\PotPlayerMini64.exe")
WINDOW = (80, 60, 1280, 760)  # x, y, width, height for every player

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")
pdh = ctypes.WinDLL("pdh")

# ---------------------------------------------------------------- process sampling


class FILETIME(ctypes.Structure):
    _fields_ = [("lo", wt.DWORD), ("hi", wt.DWORD)]


class PMC_EX(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD)] + [
        (n, ctypes.c_size_t) for n in ("PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                                       "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
                                       "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage",
                                       "PrivateUsage")]


kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]


class Proc:
    def __init__(self, pid: int):
        self.h = kernel32.OpenProcess(0x1000 | 0x0400 | 0x0010, False, pid)  # limited info | info | vm read

    def cpu_seconds(self) -> float:
        c, e, k, u = FILETIME(), FILETIME(), FILETIME(), FILETIME()
        kernel32.GetProcessTimes(self.h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(k), ctypes.byref(u))
        return ((k.hi << 32 | k.lo) + (u.hi << 32 | u.lo)) / 1e7

    def memory_mb(self) -> tuple[float, float]:
        m = PMC_EX()
        m.cb = ctypes.sizeof(m)
        psapi.GetProcessMemoryInfo(self.h, ctypes.byref(m), m.cb)
        return m.WorkingSetSize / 2**20, m.PrivateUsage / 2**20

    def close(self):
        kernel32.CloseHandle(self.h)


# ---------------------------------------------------------------- GPU sampling (PDH)

PDH_FMT_DOUBLE = 0x200
PDH_MORE_DATA = 0x800007D2


class PDH_VALUE(ctypes.Structure):
    _fields_ = [("CStatus", wt.DWORD), ("doubleValue", ctypes.c_double)]


class PDH_ITEM(ctypes.Structure):
    _fields_ = [("szName", wt.LPWSTR), ("FmtValue", PDH_VALUE)]


pdh.PdhOpenQueryW.restype = ctypes.c_ulong
pdh.PdhAddEnglishCounterW.restype = ctypes.c_ulong
pdh.PdhAddEnglishCounterW.argtypes = [ctypes.c_void_p, wt.LPCWSTR, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
pdh.PdhCollectQueryData.restype = ctypes.c_ulong
pdh.PdhCollectQueryData.argtypes = [ctypes.c_void_p]
pdh.PdhGetFormattedCounterArrayW.restype = ctypes.c_ulong
pdh.PdhGetFormattedCounterArrayW.argtypes = [ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD),
                                             ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
pdh.PdhCloseQuery.argtypes = [ctypes.c_void_p]

ENGINE_TYPES = {"3D": "gpu_3d", "VideoDecode": "gpu_decode", "VideoProcessing": "gpu_vproc", "Copy": "gpu_copy"}


class GpuSampler:
    def __init__(self, pid: int):
        self.q = ctypes.c_void_p()
        pdh.PdhOpenQueryW(None, None, ctypes.byref(self.q))
        self.counters = {}
        for key, path in (("engine", rf"\GPU Engine(pid_{pid}_*)\Utilization Percentage"),
                          ("dedicated", rf"\GPU Process Memory(pid_{pid}_*)\Dedicated Usage"),
                          ("shared", rf"\GPU Process Memory(pid_{pid}_*)\Shared Usage")):
            h = ctypes.c_void_p()
            if pdh.PdhAddEnglishCounterW(self.q, path, None, ctypes.byref(h)) == 0:
                self.counters[key] = h
        pdh.PdhCollectQueryData(self.q)  # rate counters need a first reference sample

    def _read(self, key: str) -> list[tuple[str, float]]:
        h = self.counters.get(key)
        if h is None:
            return []
        size, count = wt.DWORD(0), wt.DWORD(0)
        if pdh.PdhGetFormattedCounterArrayW(h, PDH_FMT_DOUBLE, ctypes.byref(size), ctypes.byref(count),
                                            None) != PDH_MORE_DATA:
            return []
        buf = (ctypes.c_byte * size.value)()
        if pdh.PdhGetFormattedCounterArrayW(h, PDH_FMT_DOUBLE, ctypes.byref(size), ctypes.byref(count), buf):
            return []
        items = ctypes.cast(buf, ctypes.POINTER(PDH_ITEM))
        return [(items[i].szName, items[i].FmtValue.doubleValue) for i in range(count.value)
                if items[i].FmtValue.CStatus in (0, 1)]

    def sample(self) -> dict:
        pdh.PdhCollectQueryData(self.q)
        out = {v: 0.0 for v in ENGINE_TYPES.values()}
        for name, value in self._read("engine"):
            engtype = name.rsplit("engtype_", 1)[-1]
            if engtype in ENGINE_TYPES:
                out[ENGINE_TYPES[engtype]] += value
        out["gpu_dedicated_mb"] = sum(v for _, v in self._read("dedicated")) / 2**20
        out["gpu_shared_mb"] = sum(v for _, v in self._read("shared")) / 2**20
        return out

    def close(self):
        pdh.PdhCloseQuery(self.q)


# ---------------------------------------------------------------- windows

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def main_window(pid: int):
    """Largest visible top-level window owned by pid."""
    best, best_area = None, 0

    def visit(hwnd, _):
        nonlocal best, best_area
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            r = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(r))
            area = (r.right - r.left) * (r.bottom - r.top)
            if area > best_area:
                best, best_area = hwnd, area
        return True

    user32.EnumWindows(WNDENUMPROC(visit), 0)
    return best


def wait_window(proc: subprocess.Popen, timeout: float):
    end = time.perf_counter() + timeout
    while time.perf_counter() < end and proc.poll() is None:
        hwnd = main_window(proc.pid)
        if hwnd:
            return hwnd
        time.sleep(0.005)
    return None


def place(hwnd):
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    x, y, w, h = WINDOW
    user32.SetWindowPos(hwnd, None, x, y, w, h, 0x0004 | 0x0010)  # NOZORDER | NOACTIVATE


def close_window(proc: subprocess.Popen, hwnd) -> float:
    t = time.perf_counter()
    user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        return -1.0
    return (time.perf_counter() - t) * 1000


# ---------------------------------------------------------------- one run


def run_one(player: str, exe: Path, clip: Path | None, warmup: float, sample: int) -> dict:
    report = Path(tempfile.gettempdir()) / f"yp-bench-{time.time_ns()}.json"
    if player == "yplayer":
        args = [str(exe)]
        if clip:
            args += ["--bench-report", str(report), "--bench-warmup", "3", "--bench-play", str(sample + 2), str(clip)]
    else:
        args = [str(exe), "/new"] + ([str(clip), "/seek=0"] if clip else [])
    result = {"player": player}
    t0 = time.perf_counter()
    proc = subprocess.Popen(args)
    hwnd = wait_window(proc, 30)
    if not hwnd:
        proc.kill()
        return result | {"error": "no window"}
    result["window_ms"] = (time.perf_counter() - t0) * 1000
    place(hwnd)
    time.sleep(warmup)

    p, gpu = Proc(proc.pid), GpuSampler(proc.pid)
    cpu0, w0, samples = p.cpu_seconds(), time.perf_counter(), []
    for _ in range(sample):
        time.sleep(1.0)
        ws, priv = p.memory_mb()
        samples.append({"ws_mb": ws, "private_mb": priv, **gpu.sample()})
    cpu_pct = (p.cpu_seconds() - cpu0) / (time.perf_counter() - w0) * 100
    gpu.close()
    p.close()

    result["cpu_pct"] = cpu_pct  # % of one logical core
    for key in samples[0]:
        values = [s[key] for s in samples]
        result[key] = statistics.fmean(values)
        result[key + "_max"] = max(values)
    if player == "yplayer" and clip:
        try:
            proc.wait(timeout=90)  # bench mode seeks, writes the report and exits by itself
        except subprocess.TimeoutExpired:
            proc.kill()
            result["error"] = "bench mode did not exit"
        if report.exists():
            result["internal"] = json.loads(report.read_text(encoding="utf-8"))
            report.unlink()
            result["exit_code"] = proc.returncode
    else:
        result["close_ms"] = close_window(proc, hwnd)
    return result


# ---------------------------------------------------------------- environment & summary


def ps(cmd: str) -> str:
    return subprocess.run(["powershell", "-NoProfile", "-Command", cmd], capture_output=True, text=True).stdout.strip()


def environment(potplayer: Path) -> dict:
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as k:
        cpu = winreg.QueryValueEx(k, "ProcessorNameString")[0].strip()
    gpus = json.loads(ps("Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,"
                         "CurrentHorizontalResolution,CurrentVerticalResolution,CurrentRefreshRate | ConvertTo-Json"))
    commit = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"], capture_output=True,
                            text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True,
                                text=True).stdout.strip())
    env = {"os": platform.platform(), "cpu": cpu, "logical_cores": ps("(Get-CimInstance Win32_ComputerSystem)"
                                                                       ".NumberOfLogicalProcessors"),
           "ram_gb": round(int(ps("(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory")) / 2**30, 1),
           "gpus": gpus if isinstance(gpus, list) else [gpus],
           "power_plan": ps("(powercfg /getactivescheme) -replace '.*\\((.*)\\).*','$1'"),
           "git_commit": commit + ("-dirty" if dirty else "")}
    if potplayer.exists():
        env["potplayer_version"] = ps(f"(Get-Item '{potplayer}').VersionInfo.ProductVersion")
    return env


def mean_sd(values: list[float]) -> dict | None:
    values = [v for v in values if isinstance(v, (int, float))]
    if not values:
        return None
    return {"mean": statistics.fmean(values), "sd": statistics.stdev(values) if len(values) > 1 else 0.0,
            "min": min(values), "max": max(values), "n": len(values)}


EXTERNAL = ["window_ms", "cpu_pct", "ws_mb", "ws_mb_max", "private_mb_max", "gpu_3d", "gpu_decode", "gpu_vproc",
            "gpu_copy", "gpu_dedicated_mb_max", "close_ms"]
INTERNAL = ["startup_ms", "ttff_ms", "open_to_first_frame_ms", "dropped_frames", "decoder_dropped_frames",
            "delayed_frames", "avsync_max_abs_ms"]


def summarize(runs: list[dict]) -> dict:
    groups: dict[tuple, list] = {}
    for r in runs:
        groups.setdefault((r["clip"], r["player"]), []).append(r)
    summary = {}
    for (clip, player), rs in groups.items():
        s = {k: mean_sd([r.get(k) for r in rs]) for k in EXTERNAL}
        internals = [r["internal"] for r in rs if "internal" in r]
        if internals:
            s.update({k: mean_sd([i.get(k) for i in internals]) for k in INTERNAL})
            for mode in ("exact", "keyframe"):
                s[f"seek_{mode}_ms"] = mean_sd([sk["latency_ms"] for i in internals for sk in i["seeks"]
                                                if sk["mode"] == mode])
            s["hwdec"] = sorted({i["media"]["hwdec"] or "software" for i in internals})
            s["final_states"] = sorted({i["final_state"] for i in internals})
            s["errors"] = sorted({i["error"] for i in internals if i.get("error")})
        s["run_errors"] = sorted({r["error"] for r in rs if r.get("error")})
        summary[f"{clip}|{player}"] = s
    return summary


def fmt(stat: dict | None, digits: int = 1) -> str:
    if not stat:
        return "—"
    if stat["n"] > 1 and stat["sd"] > 0:
        return f"{stat['mean']:.{digits}f} ±{stat['sd']:.{digits}f}"
    return f"{stat['mean']:.{digits}f}"


def markdown(summary: dict, meta: dict) -> str:
    lines = [f"# Benchmark {meta['label']} ({meta['date']})", "",
             f"Commit `{meta['env']['git_commit']}`, {meta['settings']['repeats']} repeat(s), "
             f"{meta['settings']['sample_s']} s sampled after {meta['settings']['warmup_s']} s warm-up, "
             f"window {WINDOW[2]}x{WINDOW[3]}. Values are mean ±sd across repeats.", "",
             "## External (identical method for every player)", "",
             "| Clip | Player | CPU % (1 core) | GPU 3D % | GPU decode % | Working set MB | Private MB (max) "
             "| GPU dedicated MB (max) | Window shown ms |",
             "|---|---|---|---|---|---|---|---|---|"]
    for key, s in summary.items():
        clip, player = key.split("|")
        lines.append(f"| {clip} | {player} | {fmt(s['cpu_pct'])} | {fmt(s['gpu_3d'])} | {fmt(s['gpu_decode'])} | "
                     f"{fmt(s['ws_mb'], 0)} | {fmt(s['private_mb_max'], 0)} | {fmt(s['gpu_dedicated_mb_max'], 0)} | "
                     f"{fmt(s['window_ms'], 0)} |")
    lines += ["", "## Y-Player internal", "",
              "| Clip | hwdec | Startup ms | First frame ms (from process start) | Open→first frame ms "
              "| Dropped | Decoder dropped | Delayed | Max A/V offset ms | Exact seek ms | Keyframe seek ms "
              "| Final state / errors |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for key, s in summary.items():
        clip, player = key.split("|")
        if player != "yplayer" or "hwdec" not in s:
            continue
        lines.append(f"| {clip} | {', '.join(s['hwdec'])} | {fmt(s['startup_ms'], 0)} | {fmt(s['ttff_ms'], 0)} | "
                     f"{fmt(s['open_to_first_frame_ms'], 0)} | {fmt(s['dropped_frames'], 1)} | "
                     f"{fmt(s['decoder_dropped_frames'], 1)} | {fmt(s['delayed_frames'], 1)} | "
                     f"{fmt(s['avsync_max_abs_ms'], 2)} | {fmt(s['seek_exact_ms'], 0)} | "
                     f"{fmt(s['seek_keyframe_ms'], 0)} | {', '.join(s['final_states'] + s['errors'])} |")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- main


def running(image: str) -> bool:
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {image}", "/NH"], capture_output=True, text=True).stdout
    return image.lower() in out.lower()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--players", default="yplayer,potplayer")
    ap.add_argument("--clips", default="", help="comma-separated corpus ids (default: all)")
    ap.add_argument("--extra", nargs="*", default=[], help="additional local files (recorded by hash, not name)")
    ap.add_argument("--no-idle", action="store_true")
    ap.add_argument("--warmup", type=float, default=4.0)
    ap.add_argument("--sample", type=int, default=8)
    ap.add_argument("--yplayer", type=Path, default=YPLAYER)
    ap.add_argument("--potplayer", type=Path, default=POTPLAYER)
    ap.add_argument("--potplayer-config", default="installed defaults (not modified by the runner)",
                    help="free-text description of PotPlayer's decoder/renderer settings for the record")
    a = ap.parse_args()

    players = {"yplayer": a.yplayer, "potplayer": a.potplayer}
    players = {p: players[p] for p in a.players.split(",")}
    for p, exe in players.items():
        if not exe.exists():
            sys.exit(f"{p} not found at {exe}")
    if "potplayer" in players and running(a.potplayer.name):
        sys.exit("PotPlayer is already running; close it first so every run starts cold.")

    manifest = json.loads((BENCH / "corpus-manifest.json").read_text())
    wanted = set(filter(None, a.clips.split(",")))
    clips = [(c["id"], BENCH / "media" / c["file"], c.get("sha256")) for c in manifest["clips"]
             if (not wanted or c["id"] in wanted) and not c.get("missing")]
    for i, extra in enumerate(a.extra):
        import hashlib
        clips.append((f"extra_{i + 1}", Path(extra), hashlib.sha256(Path(extra).read_bytes()).hexdigest()))
    if not a.no_idle:
        clips.insert(0, ("idle", None, None))

    mute = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(BENCH / "endpoint_mute.ps1")]
    was_muted = subprocess.run(mute + ["-Set", "1"], capture_output=True, text=True).stdout.strip() == "True"
    runs = []
    started = time.perf_counter()
    try:
        total = a.repeats * len(clips) * len(players)
        for rep in range(a.repeats):
            for clip_id, path, _ in clips:
                for player, exe in players.items():
                    n = len(runs) + 1
                    print(f"[{n}/{total}] repeat {rep + 1} {clip_id} {player}", flush=True)
                    r = run_one(player, exe, path, a.warmup, a.sample)
                    runs.append(r | {"clip": clip_id, "repeat": rep + 1})
    finally:
        subprocess.run(mute + ["-Set", "1" if was_muted else "0"], capture_output=True)

    date = datetime.date.today().isoformat()
    meta = {"label": a.label, "date": date, "duration_min": round((time.perf_counter() - started) / 60, 1),
            "env": environment(a.potplayer),
            "settings": {"repeats": a.repeats, "warmup_s": a.warmup, "sample_s": a.sample, "window": WINDOW,
                         "potplayer_config": a.potplayer_config},
            "corpus": {cid: sha for cid, _, sha in clips if sha}}
    summary = summarize(runs)
    out = BENCH / "results"
    out.mkdir(exist_ok=True)
    stem = out / f"{date}-{a.label}"
    stem.with_suffix(".json").write_text(json.dumps({"meta": meta, "summary": summary, "runs": runs}, indent=1))
    stem.with_suffix(".md").write_text(markdown(summary, meta), encoding="utf-8")
    print(f"wrote {stem}.json and {stem}.md in {meta['duration_min']} min")
    return 0


if __name__ == "__main__":
    sys.exit(main())

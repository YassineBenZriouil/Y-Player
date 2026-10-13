"""Generate the benchmark corpus into bench/media/ and record it in bench/corpus-manifest.json.

    python bench/make_corpus.py --ffmpeg C:/path/to/ffmpeg.exe [--only id1,id2] [--jobs 3]

Clips are synthetic (testsrc2 + temporal noise so encoders spend real bits), which keeps the
corpus reproducible and license-free. The manifest stores each file's SHA-256 so a benchmark
result can name exactly which corpus it ran on; FFmpeg builds differ, so regenerated files may
hash differently -- that is expected, re-baseline after regenerating.
"""

import argparse
import concurrent.futures
import hashlib
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MEDIA = HERE / "media"
MANIFEST = HERE / "corpus-manifest.json"

NOISE = "noise=alls=12:allf=t"
SRC = "testsrc2=size={w}x{h}:rate={fps}"
AUDIO = "sine=frequency=440:sample_rate=48000"


def video(id, w, h, fps, secs, vcodec, desc, audio="-c:a aac -b:a 192k", vf=NOISE, extra_in=(), extra_out=()):
    args = ["-f", "lavfi", "-i", SRC.format(w=w, h=h, fps=fps), "-f", "lavfi", "-i", AUDIO, *extra_in,
            "-t", str(secs), "-vf", vf, *vcodec.split(), *audio.split(), *extra_out]
    return {"id": id, "desc": desc, "args": args}


X264 = "-c:v libx264 -preset veryfast -pix_fmt yuv420p"
X265_10 = "-c:v libx265 -preset ultrafast -pix_fmt yuv420p10le -tag:v hvc1 -x265-params log-level=error"
AV1_10 = "-c:v libsvtav1 -preset 10 -pix_fmt yuv420p10le -svtav1-params svt-log-level=1"
VP9 = "-c:v libvpx-vp9 -deadline realtime -cpu-used 8 -row-mt 1 -pix_fmt yuv420p"
OPUS = "-c:a libopus -b:a 128k"

CLIPS = [
    video("h264_1080p60", 1920, 1080, 60, 25, f"{X264} -b:v 12M", "H.264 8-bit 1080p60, 12 Mb/s"),
    video("h264_2160p60_hibr", 3840, 2160, 60, 25, f"{X264} -b:v 100M -maxrate 120M -bufsize 200M",
          "H.264 8-bit 2160p60, 100 Mb/s (high bitrate)"),
    video("h264_1080p120", 1920, 1080, 120, 25, f"{X264} -b:v 20M", "H.264 8-bit 1080p120 (high frame rate)"),
    video("h264_1080p_vfr", 1920, 1080, 60, 25, f"{X264} -b:v 10M -fps_mode vfr",
          "H.264 8-bit 1080p variable frame rate (60 fps with every 3rd frame dropped)",
          vf=f"{NOISE},select='mod(n\\,3)'"),
    video("hevc10_1080p60", 1920, 1080, 60, 25, f"{X265_10} -b:v 10M", "HEVC 10-bit 1080p60, 10 Mb/s"),
    video("hevc10_2160p60", 3840, 2160, 60, 25, f"{X265_10} -b:v 40M", "HEVC 10-bit 2160p60, 40 Mb/s"),
    # Colour tags must be set on the frames (setparams) and in x265's VUI; encoder-level
    # -color_* flags alone are overridden by the untagged frames coming out of the filter.
    video("hevc10_2160p60_hdr10", 3840, 2160, 60, 25,
          "-c:v libx265 -preset ultrafast -pix_fmt yuv420p10le -tag:v hvc1 -b:v 40M -x265-params "
          "log-level=error:hdr10=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:"
          "master-display=G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,1):max-cll=1000,400",
          "HEVC 10-bit 2160p60 HDR10 (BT.2020 / PQ, mastering metadata) -> tone-mapped on SDR displays",
          vf=f"{NOISE},setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc"),
    video("hevc10_4320p30", 7680, 4320, 30, 20, f"{X265_10} -b:v 60M", "HEVC 10-bit 4320p30 (8K), 60 Mb/s"),
    video("av1_10_1080p60", 1920, 1080, 60, 25, f"{AV1_10} -b:v 8M", "AV1 10-bit 1080p60, 8 Mb/s", audio=OPUS),
    video("av1_10_2160p60", 3840, 2160, 60, 25, f"{AV1_10} -b:v 25M", "AV1 10-bit 2160p60, 25 Mb/s", audio=OPUS),
    video("vp9_1080p60", 1920, 1080, 60, 25, f"{VP9} -b:v 10M", "VP9 8-bit 1080p60, 10 Mb/s", audio=OPUS),
    video("vp9_2160p60", 3840, 2160, 60, 25, f"{VP9} -b:v 25M", "VP9 8-bit 2160p60, 25 Mb/s", audio=OPUS),
    video("multitrack_1080p30", 1920, 1080, 30, 25, f"{X264} -b:v 8M",
          "H.264 1080p30 with 2 audio tracks (AAC, Opus) and 2 subtitle tracks (SRT)",
          audio="-c:a:0 aac -b:a:0 192k -c:a:1 libopus -b:a:1 128k -c:s srt",
          extra_in=("-f", "lavfi", "-i", "sine=frequency=880:sample_rate=48000",
                    "-i", "{subs_en}", "-i", "{subs_fr}"),
          extra_out=("-map", "0:v", "-map", "1:a", "-map", "2:a", "-map", "3:s", "-map", "4:s",
                     "-metadata:s:s:0", "language=eng", "-metadata:s:s:1", "language=fre")),
]
EXT = {"vp9_1080p60": ".webm", "vp9_2160p60": ".webm"}
MP4 = {"h264_1080p60", "h264_2160p60_hibr", "h264_1080p120", "hevc10_1080p60"}


def out_path(clip_id: str) -> Path:
    return MEDIA / (clip_id + EXT.get(clip_id, ".mp4" if clip_id in MP4 else ".mkv"))


def write_subs() -> dict:
    subs = {}
    for lang, text in (("en", "Subtitle line"), ("fr", "Ligne de sous-titre")):
        p = MEDIA / f"subs_{lang}.srt"
        p.write_text("".join(f"{i + 1}\n00:00:{i:02d},000 --> 00:00:{i:02d},900\n{text} {i + 1}\n\n"
                             for i in range(25)), encoding="utf-8")
        subs[f"subs_{lang}"] = str(p)
    return subs


def make_damaged(ffmpeg: str) -> dict:
    """Copy of h264_1080p60 with 64 KiB of garbage every ~2 MiB after the header."""
    src = out_path("h264_1080p60").read_bytes()
    data = bytearray(src)
    for off in range(1 << 20, len(data) - 65536, 2 << 20):
        data[off:off + 65536] = bytes((i * 131 + 7) & 0xFF for i in range(65536))
    dst = MEDIA / "damaged_h264_1080p60.mp4"
    dst.write_bytes(data)
    return {"id": "damaged_h264_1080p60", "desc": "h264_1080p60 with 64 KiB corrupted every 2 MiB",
            "file": dst.name}


def encode(ffmpeg: str, clip: dict, subs: dict) -> None:
    args = [a.format(**subs) if "{" in a and "}" in a and "subs" in a else a for a in clip["args"]]
    cmd = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", *args, str(out_path(clip["id"]))]
    print("encoding", clip["id"], flush=True)
    subprocess.run(cmd, check=True)


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ffmpeg", default="ffmpeg")
    ap.add_argument("--only", default="")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    MEDIA.mkdir(exist_ok=True)
    subs = write_subs()
    only = set(filter(None, a.only.split(",")))
    clips = [c for c in CLIPS if not only or c["id"] in only]
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        for f in [pool.submit(encode, a.ffmpeg, c, subs) for c in clips]:
            f.result()
    entries = [{"id": c["id"], "desc": c["desc"], "file": out_path(c["id"]).name} for c in CLIPS]
    if out_path("h264_1080p60").exists():
        entries.append(make_damaged(a.ffmpeg))
    for e in entries:
        p = MEDIA / e["file"]
        e.update(size=p.stat().st_size, sha256=sha256(p)) if p.exists() else e.update(missing=True)
    MANIFEST.write_text(json.dumps({"generator": "bench/make_corpus.py", "clips": entries}, indent=2) + "\n")
    print(f"wrote {MANIFEST}")


if __name__ == "__main__":
    sys.exit(main())

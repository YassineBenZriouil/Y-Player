"""Fetch pinned third-party dependencies into deps/ (idempotent).

    python scripts/fetch_deps.py

Qt is installed with aqtinstall (which verifies Qt's published checksums); libmpv is
downloaded from a pinned release and verified against a pinned SHA-256.
"""

import hashlib
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEPS = ROOT / "deps"

QT_VERSION = "6.9.3"
QT_ARCH = "win64_msvc2022_64"
QT_DIR = DEPS / "qt" / QT_VERSION / "msvc2022_64"
QT_ARCHIVES = ["qtbase", "qttools"]  # qtbase: Core/Gui/Widgets/Test + windeployqt; qttools: lupdate/lrelease
QT_STAMP = DEPS / "qt" / ".complete"
AQT_PIN = "aqtinstall==3.3.0"
# download.qt.io drops TLS connections under parallel load; serialize and retry.
AQT_CONFIG = """[aqt]
concurrency: 1
[requests]
max_retries_on_connection_error: 10
retry_backoff: 2
"""

MPV_VERSION = "20261011-git-accb7b2821"
MPV_URL = (
    "https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20261011/"
    f"mpv-dev-x86_64-{MPV_VERSION}.7z"
)
MPV_SHA256 = "cf63b4ce66e03e5e70acf2648cbc3177656c77c5b9583e16d83efc58801182b7"
MPV_DIR = DEPS / "mpv"

# Windows' bundled bsdtar extracts 7z (incl. BCJ2), so no 7-Zip install is needed.
TAR = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "tar.exe"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, check=True, cwd=cwd)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch_qt() -> None:
    want = f"{QT_VERSION} {QT_ARCH} {' '.join(QT_ARCHIVES)}"
    if QT_STAMP.exists() and QT_STAMP.read_text().strip() == want:
        print(f"Qt {QT_VERSION}: present")
        return
    venv = DEPS / ".venv"
    py = venv / "Scripts" / "python.exe"
    if not py.exists():
        run([sys.executable, "-m", "venv", str(venv)])
    run([str(py), "-m", "pip", "install", "--quiet", "--disable-pip-version-check", AQT_PIN])
    config = DEPS / "aqt.ini"
    config.write_text(AQT_CONFIG)
    for attempt in range(1, 4):
        try:
            run([str(py), "-m", "aqt", "-c", str(config), "install-qt", "windows", "desktop", QT_VERSION,
                 QT_ARCH, "-O", str(DEPS / "qt"), "--archives", *QT_ARCHIVES], cwd=DEPS)  # aqt writes its log to cwd
            break
        except subprocess.CalledProcessError:
            if attempt == 3:
                raise
            print(f"Qt download failed, retrying ({attempt}/3)")
    if not (QT_DIR / "bin" / "Qt6Core.dll").exists():
        sys.exit(f"Qt install finished but {QT_DIR} is incomplete")
    QT_STAMP.write_text(want + "\n")


def fetch_mpv() -> None:
    stamp = MPV_DIR / ".version"
    if stamp.exists() and stamp.read_text().strip() == MPV_VERSION:
        print(f"libmpv {MPV_VERSION}: present")
        return
    downloads = DEPS / "_downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    archive = downloads / MPV_URL.rsplit("/", 1)[1]
    if not archive.exists() or sha256(archive) != MPV_SHA256:
        print(f"downloading {MPV_URL}")
        tmp = archive.with_suffix(".part")
        urllib.request.urlretrieve(MPV_URL, tmp)
        tmp.replace(archive)
    actual = sha256(archive)
    if actual != MPV_SHA256:
        archive.unlink()
        sys.exit(f"libmpv checksum mismatch: expected {MPV_SHA256}, got {actual}")
    MPV_DIR.mkdir(parents=True, exist_ok=True)
    run([str(TAR), "-xf", str(archive), "-C", str(MPV_DIR)])
    if not (MPV_DIR / "libmpv-2.dll").exists():
        sys.exit("libmpv archive did not contain libmpv-2.dll")
    stamp.write_text(MPV_VERSION + "\n")


if __name__ == "__main__":
    if sys.platform != "win32":
        sys.exit("Y-Player dependencies are Windows-only")
    fetch_qt()
    fetch_mpv()
    print("dependencies ready")

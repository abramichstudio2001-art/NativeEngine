# ============================================================================
#  Native Engine — build.py
#  Compiles the C++ core into a shared library. Zero external dependencies:
#  only needs a C++17 compiler (g++ / clang++ / cl.exe) and Python 3.
# ============================================================================
import os
import platform
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "engine", "src")
BUILD_DIR = os.path.join(ROOT, "build")

SOURCES = ["ne_api.cpp"]
LIB_NAME = {
    "Linux": "libNativeEngine.so",
    "Darwin": "libNativeEngine.dylib",
    "Windows": "NativeEngine.dll",
}


def _compiler():
    for cc in (os.environ.get("CXX"), "g++", "clang++", "c++"):
        if cc and shutil.which(cc):
            return cc
    cl = shutil.which("cl")
    if cl:
        return "cl"
    raise RuntimeError(
        "No C++ compiler found. Install g++ (Linux), clang (macOS: xcode-select "
        "--install) or MSVC (Windows) and make sure it is on PATH."
    )


def _artifact():
    return os.path.join(BUILD_DIR, LIB_NAME[platform.system()])


def _srcstamp():
    latest = 0.0
    for name in os.listdir(SRC_DIR):
        p = os.path.join(SRC_DIR, name)
        if name.endswith((".h", ".cpp")):
            latest = max(latest, os.path.getmtime(p))
    return latest


def is_build_stale():
    lib = _artifact()
    if not os.path.exists(lib):
        return True
    return _srcstamp() > os.path.getmtime(lib)


def build(verbose=True, force=False):
    """Compile the native core if needed. Returns path to the shared library."""
    os.makedirs(BUILD_DIR, exist_ok=True)
    lib = _artifact()
    if not force and not is_build_stale():
        if verbose:
            print(f"[native-engine] up-to-date: {os.path.relpath(lib, ROOT)}")
        return lib

    cc = _compiler()
    system = platform.system()
    sources = [os.path.join(SRC_DIR, s) for s in SOURCES]
    t0 = time.time()
    if system == "Windows":
        cmd = [cc, "/std:c++17", "/O2", "/MD", "/EHsc", "/D_USRDLL"]
        cmd += ["/I" + SRC_DIR] + sources
        cmd += ["/LD", "/Fe:" + lib, "/link", "/OUT:" + lib]
    else:
        pic = "-fPIC"
        cmd = [cc, "-std=c++17", "-O3", "-ffast-math", "-funroll-loops",
               "-pthread", pic, "-shared", "-I" + SRC_DIR]
        if system == "Darwin":
            cmd += ["-dynamiclib"]
        cmd += sources + ["-o", lib]
    if verbose:
        print(f"[native-engine] compiling with {cc} ...")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr[-8000:] if proc.stderr else "compile failed\n")
        raise RuntimeError("Native engine build failed — see compiler output above.")
    if verbose:
        print(f"[native-engine] built in {time.time() - t0:.1f}s -> "
              f"{os.path.relpath(lib, ROOT)}")
    return lib


if __name__ == "__main__":
    path = build(force="--force" in sys.argv)
    print(path)

# ============================================================================
#  Native Engine — build.py
#  Compiles the C++ core into a shared library. Zero external dependencies:
#  only needs a C++17 compiler (g++ / clang++ / cl.exe) and Python 3.
#
#  Windows notes:
#   - Works with ANY MinGW-w64 build, including "win32 threads model"
#     distributions where std::thread cannot link (the engine uses the
#     native Win32 thread API instead — see engine/src/ne_threads.h).
#   - The GCC path links the C++ runtime statically so the resulting DLL
#     loads without libgcc/libstdc++/winpthread on PATH.
#   - For MSVC (cl.exe) run from a "Developer Command Prompt" shell.
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

SOURCES = ["ne_api.cpp", "ne_editor.cpp"]
LIB_NAME = {
    "Linux": "libNativeEngine.so",
    "Darwin": "libNativeEngine.dylib",
    "Windows": "NativeEngine.dll",
}


def _is_msvc(cc):
    return os.path.basename(cc).lower() in ("cl", "cl.exe")


def _compiler():
    for cc in (os.environ.get("CXX"), "g++", "clang++", "c++"):
        if cc and shutil.which(cc):
            return cc
    cl = shutil.which("cl")
    if cl:
        return cl
    raise RuntimeError(
        "No C++ compiler found. Install one:\n"
        "  Windows: MSYS2 (pacman -S mingw-w64-x86_64-gcc) or Visual Studio\n"
        "  macOS:   xcode-select --install\n"
        "  Linux:   sudo apt install g++"
    )


def _name():
    return LIB_NAME[platform.system()]


def _prefab():
    """A prebuilt shared library shipped in bin/ (used as-is, no compiler needed)."""
    p = os.path.join(ROOT, "bin", _name())
    return p if os.path.exists(p) else None


def _artifact():
    p = _prefab()
    return p if p else os.path.join(BUILD_DIR, _name())


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


def _gcc_commands(cc, system, lib):
    srcs = [os.path.join(SRC_DIR, s) for s in SOURCES]
    base_tail = ["-I" + SRC_DIR] + srcs + ["-o", lib]
    shared = "-dynamiclib" if system == "Darwin" else "-shared"
    static = ["-static-libgcc", "-static-libstdc++", "-static"] if system == "Windows" else []
    threads = [] if system == "Windows" else ["-pthread"]

    ladder = [
        ["-std=c++17", "-O3", "-ffast-math", "-funroll-loops"],
        ["-std=c++17", "-O3"],
        ["-std=c++17", "-O2"],
        ["-std=gnu++17", "-O2"],
    ]
    for flags in ladder:
        yield [cc] + flags + threads + static + ["-fPIC", shared] + base_tail


def _msvc_command(cc, lib):
    srcs = [os.path.join(SRC_DIR, s) for s in SOURCES]
    stem = os.path.splitext(lib)[0]
    return ([cc, "/nologo", "/std:c++17", "/O2", "/MD", "/EHsc", "/utf-8",
             "/D_WIN32_WINNT=0x0A00", "/I" + SRC_DIR] + srcs +
            ["/LD", f"/Fo:{stem}.obj.", f"/Fe:{os.path.basename(lib)}",
             "/link", f"/OUT:{lib}"])


def build(verbose=True, force=False):
    """Compile the native core if needed. Returns path to the shared library."""
    prefab = _prefab()
    if prefab and not force:
        if verbose:
            print(f"[native-engine] using prebuilt library: {os.path.relpath(prefab, ROOT)}")
        return prefab

    os.makedirs(BUILD_DIR, exist_ok=True)
    lib = os.path.join(BUILD_DIR, _name())
    if not force and not is_build_stale():
        if verbose:
            print(f"[native-engine] up-to-date: {os.path.relpath(lib, ROOT)}")
        return lib

    # remove stale/partial output so the linker never trips on it
    for p in (lib, os.path.splitext(lib)[0] + ".lib"):
        try:
            if os.path.exists(p):
                os.remove(p)
        except PermissionError:
            raise RuntimeError(
                f"{p} is locked — close any running Python/preview that "
                "loaded it, then retry.")

    cc = _compiler()
    system = platform.system()
    t0 = time.time()
    if verbose:
        print(f"[native-engine] compiling with {cc} ...")

    errors = []
    if _is_msvc(cc):
        cmds = [_msvc_command(cc, lib)]
    else:
        cmds = list(_gcc_commands(cc, system, lib))

    for cmd in cmds:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              cwd=os.path.dirname(lib))
        if proc.returncode == 0:
            if verbose:
                print(f"[native-engine] built in {time.time() - t0:.1f}s -> "
                      f"{os.path.relpath(lib, ROOT)}")
            return lib
        err = ((proc.stderr or "") + "\n" + (proc.stdout or "")).strip()
        errors.append((cmd, err))

    # surface the REAL compiler/linker error in the traceback, not just "failed"
    last_cmd, last_err = errors[-1]
    def _fmt(err):
        lines = [l for l in err.splitlines() if "error" in l.lower()
                 or "undefined reference" in l.lower()]
        return "\n".join(lines[:12]) if lines else err[-1500:]
    summary = "\n\n".join(f"> {' '.join(c)}\n{_fmt(e)}" for c, e in errors[:1])
    raise RuntimeError(
        "Native engine build failed.\n\n" + summary
        + "\n\n(On Windows: this project needs no special MinGW flavor — "
          "any g++ works; if 'ld returned 1' mentions Permission denied or "
          "file format, delete the build/ folder and close other Python "
          "processes, then run again.)")


if __name__ == "__main__":
    path = build(force="--force" in sys.argv)
    print(path)

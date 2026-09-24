// ============================================================================
//  Native Engine — ne_ide.h
//  Built-in IDE for the RTX editor: a small code widget (lines, gutter,
//  caret, wheel scroll, click-to-position, Python/C++ syntax highlighting),
//  a project file browser rooted at <repo>/projects, and a child-process
//  runner (spawn + piped console + kill) so F5-style "RUN" actually
//  executes the script and streams its output into the console pane.
//
//  Zero dependencies, links everywhere the engine does: Win32 uses
//  CreateProcess/CreatePipe/Toolhelp for the tree-kill; POSIX uses
//  fork/execvp over /bin/sh with a process group so STOP kills the child
//  AND its toolchain descendants. No std::thread (ne_threads shim).
// ============================================================================
#pragma once

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ne_threads.h"

#if defined(_WIN32)
    #include <direct.h>
    #include <tlhelp32.h>
#else
    #include <dirent.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace ed {

struct Ctx;                       // immediate-mode canvas (defined in ne_editor.cpp)
void ui_text(Ctx& u, int x, int y, const char* s, int scale, uint32_t c);
void ui_rect(Ctx& u, int x, int y, int w, int h, uint32_t c, float a = 1.0f);
void ui_frame(Ctx& u, int x, int y, int w, int h, uint32_t c);
bool ui_in(Ctx& u, int x, int y, int w, int h);
bool ui_click(Ctx& u);
int  ui_mx(Ctx& u);
int  ui_my(Ctx& u);

// shared palette (ne_editor.cpp defines the same names for the main UI via these)
constexpr uint32_t BG = 0x0F1219, PANEL = 0x151A24, PANEL2 = 0x1B2230,
                   TEXT = 0xC9D4E4, DIM = 0x6B7A93, ACCENT = 0x35E7FF,
                   PINK = 0xFF4FB2, GOLD = 0xFFC93D, GREEN = 0x3DFF8E,
                   TRACK = 0x0A0D13,
                   CODE_STR = 0xFF9BCE, CODE_NUM = 0xFFC93D, CODE_KW = 0x35E7FF,
                   CODE_CMT = 0x50607A, CODE_PP = 0x3DFF8E, GUT_NUM = 0x42506A;

// ---------------------------------------------------------------------------
//  filesystem helpers
// ---------------------------------------------------------------------------
inline std::string ide_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a[a.size() - 1];
    bool sep = (last == '/' || last == '\\');
    return a + (sep ? "" : "/") + b;
}
inline bool ide_exists(const std::string& p) {
#ifdef _WIN32
    DWORD att = GetFileAttributesA(p.c_str());
    return att != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
#endif
}
inline std::string ide_dirname(std::string p) {
    size_t s = p.find_last_of("/\\");
    if (s == std::string::npos) return ".";
    return p.substr(0, s);
}
inline std::string ide_filename(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

// walk up from the executable looking for the repo root (dir with engine/src)
inline std::string ide_repo_root() {
    char buf[1024] = {0};
#ifdef _WIN32
    GetModuleFileNameA(nullptr, buf, 1023);
#else
    ssize_t n = readlink("/proc/self/exe", buf, 1023);
    if (n > 0) buf[n] = 0;
#endif
    std::string dir = buf[0] ? ide_dirname(std::string(buf)) : std::string(".");
    for (int up = 0; up < 5; ++up) {
        if (ide_exists(ide_join(ide_join(dir, "engine"), "src/ne_renderer.h"))) return dir;
        std::string nu = ide_dirname(dir);
        if (nu == dir || nu.empty()) break;
        dir = nu;
    }
    if (ide_exists("engine/src/ne_renderer.h")) return ".";
    return ".";
}

inline std::vector<std::string> ide_listdir(const std::string& dir) {
    std::vector<std::string> out;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(ide_join(dir, "*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            out.push_back(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR* d = opendir(dir.c_str());
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            out.push_back(n);
        }
        closedir(d);
    }
#endif
    std::sort(out.begin(), out.end());
    if (out.size() > 32) out.resize(32);
    return out;
}

inline bool ide_read_file(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    out.clear();
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}
inline bool ide_write_file(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
//  child process: spawn with piped stdout+stderr, reader thread -> log lines
// ---------------------------------------------------------------------------
struct ChildRun {
    std::vector<std::string> log;
    ne::SpinLock mtx;
    volatile int running = 0;
    volatile int exit_code = -1;
    bool done = false;                    // exited since last poll
    std::string pending;                  // partial last line

#ifdef _WIN32
    HANDLE rd = nullptr, wr = nullptr;
    PROCESS_INFORMATION pi{};
#else
    int fds[2] = {-1, -1};
    pid_t pid = -1;
#endif
    ne::thread_handle th{};

    static void push_line(ChildRun* c, std::string s) {
        if (!s.empty() && s[s.size() - 1] == '\r') s.erase(s.size() - 1);
        if (s.empty() && !c->log.empty()) return;
        ne::SpinGuard g(c->mtx);
        c->log.push_back(std::move(s));
        if (c->log.size() > 400) c->log.erase(c->log.begin(), c->log.begin() + 100);
    }

#ifdef _WIN32
    static unsigned __stdcall reader(void* v) {
        ChildRun* c = (ChildRun*)v;
        char buf[2048];
        DWORD got = 0;
        while (ReadFile(c->rd, buf, sizeof buf, &got, nullptr) && got > 0) {
            for (DWORD i = 0; i < got; ++i) {
                char ch = buf[i];
                if (ch == '\n') { push_line(c, c->pending); c->pending.clear(); }
                else if (ch != '\r') c->pending.push_back(ch);
            }
        }
        if (!c->pending.empty()) { push_line(c, c->pending); c->pending.clear(); }
        return 0;
    }
#else
    static void reader(void* v) {
        ChildRun* c = (ChildRun*)v;
        char buf[2048];
        for (;;) {
            struct pollfd pf{c->fds[0], POLLIN, 0};
            int pr = poll(&pf, 1, 200);
            if (pr < 0) { if (errno == EINTR) continue; break; }
            if (pr == 0) continue;
            ssize_t n = read(c->fds[0], buf, sizeof buf);
            if (n <= 0) break;
            for (ssize_t i = 0; i < n; ++i) {
                char ch = buf[i];
                if (ch == '\n') { push_line(c, c->pending); c->pending.clear(); }
                else c->pending.push_back(ch);
            }
        }
        if (!c->pending.empty()) { push_line(c, c->pending); c->pending.clear(); }
    }
#endif

    std::string cwd;   // child working directory (repo root)

    bool start(const std::string& cmdline, std::string& err) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        if (!CreatePipe(&rd, &wr, &sa, 1 << 16)) { err = "CreatePipe failed"; return false; }
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOA si{};
        si.cb = sizeof si;
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = si.hStdError = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        std::string mut = cmdline;
        if (!CreateProcessA(nullptr, &mut[0], nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                            nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi)) {
            DWORD e = GetLastError();
            char b[96]; std::snprintf(b, sizeof b, "CreateProcess failed (err %u)", e);
            err = b;
            CloseHandle(rd); CloseHandle(wr); rd = wr = nullptr;
            return false;
        }
        CloseHandle(wr); wr = nullptr;
        running = 1; done = false; exit_code = -1;
        th = (ne::thread_handle)(uintptr_t)
             _beginthreadex(nullptr, 0, reader, this, 0, nullptr);
        return true;
#else
        if (pipe(fds) != 0) { err = "pipe() failed"; return false; }
        pid_t p = fork();
        if (p < 0) { err = "fork() failed"; ::close(fds[0]); ::close(fds[1]); return false; }
        if (p == 0) {
            // child: new process group (kill tree via kill(-pid)), stdio -> pipe
            setpgid(0, 0);
            if (!cwd.empty()) { if (::chdir(cwd.c_str()) != 0) {} }
            dup2(fds[1], 1); dup2(fds[1], 2);
            ::close(fds[0]); ::close(fds[1]);
            execlp("/bin/sh", "sh", "-c", cmdline.c_str(), (char*)nullptr);
            _exit(127);
        }
        ::close(fds[1]);
        pid = p; running = 1; done = false; exit_code = -1;
        th = ne::spawn_thread(reader, this);
        return true;
#endif
    }

    bool alive() {
        if (!running) return false;
#ifdef _WIN32
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
            exit_code = (int)code; running = 0; done = true;
            return false;
        }
        return true;
#else
        int st = 0;
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) {
            exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
            running = 0; done = true;
            return false;
        }
        return true;
#endif
    }

    void kill() {
        if (!running) return;
#ifdef _WIN32
        DWORD mine = pi.dwProcessId;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe{}; pe.dwSize = sizeof pe;
            if (Process32First(snap, &pe)) {
                do {
                    if (pe.th32ParentProcessID == mine) {
                        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hp) { TerminateProcess(hp, 3); CloseHandle(hp); }
                    }
                } while (Process32Next(snap, &pe));
            }
            CloseHandle(snap);
        }
        TerminateProcess(pi.hProcess, 3);
#else
        setpgid(pid, pid);            // best-effort if not yet applied
        killpg(pid, SIGKILL);
#endif
        running = 0; done = true; exit_code = 130;
    }

    void join_reader() {
#ifdef _WIN32
        uintptr_t h = (uintptr_t)th;
        if (h) { WaitForSingleObject((HANDLE)h, INFINITE); CloseHandle((HANDLE)h); th = 0; }
#else
        ne::join_thread(th);
#endif
    }

    void finish() {   // call at shutdown: make sure nothing lingers
        if (running) kill();
        join_reader();
#ifdef _WIN32
        if (pi.hProcess) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); pi = {}; }
        if (rd) CloseHandle(rd); if (wr) CloseHandle(wr);
        rd = wr = nullptr;
#else
        if (fds[0] >= 0) { ::close(fds[0]); fds[0] = -1; }
#endif
    }

    std::vector<std::string> snapshot() {
        ne::SpinGuard g(mtx);
        return log;
    }
};

// ---------------------------------------------------------------------------
//  templates seeded into projects/ on first use
// ---------------------------------------------------------------------------
inline const char* TPL_PY = R"PY(# NEON ORBS -- RTX game sample for the Native Engine IDE.
# Press RUN (Ctrl+R): this script streams a path-traced game in a native
# window. Ray tracing is progressive: the picture starts noisy and sharpens
# while you play, and dragging objects never stalls the frame.
import os, sys, math, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
import native_engine as ne

RTW, RTH = 480, 270          # trace at low res, upscale -> instant feedback
WINW, WINH = 960, 540

app = ne.App(RTW, RTH, bounces=3)
app.ground(albedo=(0.82, 0.84, 0.9), roughness=0.28, checker=0.55)
floor_mat = app.emissive((0.1, 0.9, 1.0), 4.0)
app.quad(floor_mat, (-6, 0.01, -6), (12, 0, 0), (0, 0, 12))
mats = [app.metal((1.0, 0.72, 0.25), 0.12),
        app.metal((0.9, 0.25, 0.9), 0.05),
        app.glass((0.97, 0.99, 1.0), 1.52),
        app.metal((0.3, 0.95, 0.5), 0.2)]
balls = [app.sphere(mats[i], ((i - 1.5) * 2.2, 1.1, -2.0 + (i % 2))) for i in range(4)]

win = ne.Window(WINW, WINH, "NEON ORBS - Native Engine RTX")
t0 = time.time()
app.begin_stream(32)
while win.pump():
    t = time.time() - t0
    for i, b in enumerate(balls):
        x = (i - 1.5) * 2.2
        z = -2.0 + math.cos(t * (0.7 + 0.2 * i)) * 2.5
        y = 1.1 + abs(math.sin(t * (1.4 + 0.15 * i))) * 1.3
        b.set_transform(pos=(x, y, z))
    app.camera(pos=(math.sin(t * 0.15) * 6.0, 2.4, math.cos(t * 0.15) * 6.0),
               target=(0, 1.0, -1))
    app.begin_stream(32)          # edit -> re-queue the tile stream
    busy, passes = app.stream_frame(4)
    win.present(app.blit(WINW, WINH))
    print(f"\rframe {t:6.1f}s  passes={passes:2d}  busy={busy}", end="")
    if win.key_pressed(ne.K_ESCAPE):
        break
print("\nbye - Native Engine")
)PY";

inline const char* TPL_CPP = R"CPP(// Native Engine C++ sample -- compiled by the IDE with your local g++.
// Include the engine headers directly: one file, one compile, no project files.
#include "ne_renderer.h"
#include "ne_window.h"
#include <cmath>
using namespace ne;

int main() {
    Renderer rt{0, 3};                 // auto threads, 3 bounces
    rt.resize(640, 360);
    Scene& s = rt.scene;
    Material floor; floor.albedo = Vec3(0.85f, 0.85f, 0.88f); floor.roughness = 0.3f;
    floor.checker = 0.6f;
    Material metal; metal.albedo = Vec3(1.0f, 0.72f, 0.25f); metal.metallic = 1;
    metal.roughness = 0.12f;
    int mf = s.add_material(floor), mm = s.add_material(metal);
    Object pl; pl.type = ObjType::Plane; pl.mat = mf; pl.d = 0; s.add_object(pl);
    Object sp; sp.type = ObjType::Sphere; sp.mat = mm; sp.radius = 1.0f;
    s.add_object(sp);
    int is = (int)s.objects.size() - 1;
    s.sky.top = Vec3(0.25f, 0.45f, 0.85f);
    s.sky.horizon = Vec3(0.9f, 0.72f, 0.58f);
    s.sky.sun_dir = normalize(Vec3(0.42f, 0.55f, 0.28f));
    s.sky.sun_intensity = 4.0f;

    Window win;
    win.create("Native Engine - C++ sample", 960, 540);
    std::vector<uint8_t> frame((size_t)win.width * win.height * 4);
    double t0 = win.now();
    while (win.pump()) {
        float t = (float)(win.now() - t0);
        s.objects[is].pos = Vec3(std::sin(t) * 2.2f, 1.1f + std::fabs(std::sin(2 * t)),
                                 std::cos(t) * 2.2f);
        s.camera.pos = Vec3(std::sin(t * 0.15f) * 6.0f, 2.5f, std::cos(t * 0.15f) * 6.0f);
        s.camera.target = Vec3(0, 1, 0);
        rt.begin_stream(1, 16);
        while (rt.stream_step(4000)) {}
        rt.present_stream();
        rt.denoise();
        // upscale RT -> window
        const uint8_t* src = rt.pixels();
        int sw = rt.view_w(), sh = rt.view_h();
        for (int y = 0; y < win.height; ++y)
            for (int x = 0; x < win.width; ++x) {
                const uint8_t* p = src + (((size_t)(y * sh / win.height) * sw +
                                          (size_t)(x * sw / win.width)) * 4);
                uint8_t* d = &frame[((size_t)y * win.width + x) * 4];
                d[0] = p[0]; d[1] = p[1]; d[2] = p[2]; d[3] = 255;
            }
        win.present(frame.data());
        if (win.key_pressed[0x1B]) break;   // Esc
    }
    return 0;
}
)CPP";

// ---------------------------------------------------------------------------
//  the IDE panel itself
// ---------------------------------------------------------------------------
struct Ide {
    bool vis = false;
    std::string root, dir, path;         // repo root, projects dir, current file
    std::vector<std::string> lines{""};
    int cr = 0, cc = 0;                  // caret row/col (byte index)
    int scroll_r = 0, scroll_c = 0;
    bool dirty = false;
    bool listing = false;                // OPEN dropdown showing
    float console_frac = 0.36f;
    ChildRun run;
    double caret_blink_t = 0.0;
    std::string note;

    bool is_cpp() const {
        size_t d = path.find_last_of('.');
        std::string ext = d == std::string::npos ? "" : path.substr(d);
        return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".cc";
    }

    void init() {
        root = ide_repo_root();
        dir = ide_join(root, "projects");
#ifdef _WIN32
        _mkdir(dir.c_str());
#else
        ::mkdir(dir.c_str(), 0777);
#endif
        auto files = ide_listdir(dir);
        bool has_src = false;
        for (auto& f : files) {
            std::string e = f.substr(f.find_last_of('.') + 1);
            if (e == "py" || e == "cpp") has_src = true;
        }
        if (!has_src) {
            ide_write_file(ide_join(dir, "neon_orbs.py"), TPL_PY);
            ide_write_file(ide_join(dir, "orbit.cpp"), TPL_CPP);
            files = ide_listdir(dir);
        }
        for (auto& f : files) {                 // open first source file
            std::string e = f.size() > 3 ? f.substr(f.find_last_of('.')) : "";
            if (e == ".py" || e == ".cpp") { load(ide_join(dir, f)); break; }
        }
    }
    ~Ide() { run.finish(); }

    bool load(const std::string& p) {
        std::string data;
        if (!ide_read_file(p, data)) { note = "cannot read " + ide_filename(p); return false; }
        lines.clear(); lines.push_back("");
        for (char ch : data) {
            if (ch == '\n') lines.push_back("");
            else if (ch != '\r') lines.back().push_back(ch);
        }
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
        if (lines.empty()) lines.push_back("");
        path = p; dirty = false; cr = cc = scroll_r = scroll_c = listing = false;
        clamp_caret();
        return true;
    }
    bool save() {
        if (path.empty()) {                       // new file: pick a name
            int k = 1;
            char nm[32];
            do { std::snprintf(nm, sizeof nm, "/script_%d.py", k++); }
            while (ide_exists(dir + nm));
            path = dir + nm;
        }
        std::string data;
        for (auto& l : lines) { data += l; data += "\n"; }
        if (!ide_write_file(path, data)) { note = "save failed"; return false; }
        dirty = false; note = "saved " + ide_filename(path);
        return true;
    }

    void clamp_caret() {
        if (cr < 0) cr = 0;
        if (cr >= (int)lines.size()) cr = (int)lines.size() - 1;
        int len = (int)lines[cr].size();
        if (cc > len) cc = len;
        if (cc < 0) cc = 0;
    }

    // ------------------------------------------------------------------ keys
    void type_char(int c) {
        if (c == 9) {                              // tab -> 4 spaces
            insert("    ");
        } else if (c == 13 || c == 10) {           // enter, keep indentation
            std::string& cur = lines[cr];
            size_t i = 0;
            while (i < cur.size() && cur[i] == ' ') ++i;
            std::string ind = cur.substr(0, i);
            std::string tail = cur.substr(cc);
            cur = cur.substr(0, cc);
            lines.insert(lines.begin() + cr + 1, ind + tail);
            ++cr; cc = (int)ind.size();
            dirty = true;
        } else if (c == 8) {                       // backspace
            if (cc > 0) {
                lines[cr].erase(lines[cr].begin() + cc - 1);
                --cc; dirty = true;
            } else if (cr > 0) {
                cc = (int)lines[cr - 1].size();
                lines[cr - 1] += lines[cr];
                lines.erase(lines.begin() + cr);
                --cr; dirty = true;
            }
        } else if (c == 3 || c == 1) {             // ctrl-a: home
            cc = 0;
        } else if (c >= 32 && c < 127) {
            char s[2] = {(char)c, 0};
            insert(s);
        }
        clamp_caret();
    }
    void insert(const std::string& s) {
        lines[cr].insert(cc, s);
        cc += (int)s.size();
        dirty = true;
    }
    void key(int vk) {
        switch (vk) {
            case 0x25: --cc; break;                       // left
            case 0x27: ++cc; break;                       // right
            case 0x26:                                               // up
                if (cr > 0) { --cr; if (cc > (int)lines[cr].size()) cc = (int)lines[cr].size(); }
                break;
            case 0x28:                                               // down
                if (cr + 1 < (int)lines.size()) {
                    ++cr; if (cc > (int)lines[cr].size()) cc = (int)lines[cr].size();
                }
                break;
            case 0x24: cc = 0; break;                                // home
            case 0x23: cc = (int)lines[cr].size(); break;            // end
            case 0x2E:                                               // delete
                if (cc < (int)lines[cr].size()) lines[cr].erase(cc, 1);
                else if (cr + 1 < (int)lines.size()) {
                    lines[cr] += lines[cr + 1]; lines.erase(lines.begin() + cr + 1);
                }
                dirty = true; break;
            case 0x21: cr -= 14; dirty = false; break;               // pgup
            case 0x22: cr += 14; break;                              // pgdn
        }
        clamp_caret();
    }

    // ------------------------------------------------------------------ run
    std::string run_note;
    void do_run() {
        if (run.running) { note = "already running - STOP first"; return; }
        run.join_reader();                       // reap previous reader
        run.cwd = root;
        { ne::SpinGuard g(run.mtx); run.log.clear(); }
        save();
        bool cpp = is_cpp();
        std::string out = ide_join(dir, "_out");
#ifdef _WIN32
        out += ".exe";
        std::string py = "python \"" + path + "\"";
        std::string cmd = cpp
            ? "cmd /c g++ -std=c++17 -O2 -ffast-math -I \"" + ide_join(root, "engine/src") +
              "\" \"" + path + "\" -o \"" + out + "\" && \"" + out + "\""
            : py;
#else
        std::string py = "python3 \"" + path + "\"";
        std::string cmd = cpp
            ? "g++ -std=c++17 -O2 -ffast-math -pthread -I \"" + ide_join(root, "engine/src") +
              "\" \"" + path + "\" -o \"" + out + "\" && \"" + out + "\""
            : py;
#endif
        if (!cpp) {   // try plain `python` first on POSIX too, then python3
#ifdef _WIN32
            std::string err;
            if (run.start(cmd, err)) { note = "running " + ide_filename(path); return; }
            note = err; ChildRun::push_line(&run, "[ide] " + err);
#else
            std::string err;
            if (run.start(py, err)) { note = "running " + ide_filename(path); return; }
            if (run.start(cmd, err)) { note = "running " + ide_filename(path); return; }
            note = err; ChildRun::push_line(&run, "[ide] " + err);
#endif
            return;
        }
        std::string err;
#ifdef _WIN32
        if (run.start(cmd, err)) { note = "compiling " + ide_filename(path); return; }
#else
        if (run.start(cmd, err)) { note = "compiling " + ide_filename(path); return; }
#endif
        note = err.empty() ? "run failed" : err;
        ChildRun::push_line(&run, "[ide] " + note);
    }
    void do_stop() {
        if (run.running) { run.kill(); run.join_reader(); note = "stopped"; }
    }

    // ------------------------------------------------------------- painting
    static bool in_set(const char* list, const std::string& w) {
        const char* p = list;
        size_t n = w.size();
        while (*p) {
            const char* q = p; while (*q && *q != ' ') ++q;
            if (q - p == (int)n && w.compare(0, n, p, n) == 0) return true;
            p = (*q) ? q + 1 : q;
        }
        return false;
    }

    // draw one highlighted line; returns x after last run
    int draw_line(Ctx& u, const std::string& s, int x, int y, int cw) {
        static const char* KW_PY = "def class import from return if elif else for while in not "
            "and or None True False print try except finally with as lambda global nonlocal "
            "raise yield assert pass break continue del is range len int float str list dict set";
        static const char* KW_C = "int float double void auto const static struct class return if "
            "else for while do break continue new delete true false nullptr this public private "
            "protected namespace using include template typename inline virtual override bool char "
            "unsigned size_t std";
        bool cpp = is_cpp();
        size_t i = 0;
        while (i < s.size()) {
            char ch = s[i];
            size_t j; uint32_t col = TEXT;
            if (ch == ' ') { ++i; x += cw; continue; }
            if ((!cpp && ch == '#') || (cpp && i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/')) {
                j = s.size(); col = CODE_CMT;
            } else if (cpp && i == 0 && ch == '#') {
                j = s.size(); col = CODE_PP;
            } else if (ch == '"' || ch == '\'') {
                j = i + 1;
                while (j < s.size() && s[j] != ch) ++j;
                if (j < s.size()) ++j;
                col = CODE_STR;
            } else if (std::isdigit((unsigned char)ch)) {
                j = i;
                while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '.')) ++j;
                col = CODE_NUM;
            } else {
                j = i;
                while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == '.')) ++j;
                std::string w = s.substr(i, j - i);
                col = in_set(cpp ? KW_C : KW_PY, w) ? CODE_KW : TEXT;
            }
            if (j <= i) j = i + 1;                 // safety: always progress
            std::string run = s.substr(i, j - i);
            ui_text(u, x, y, run.c_str(), 2, col);
            x += cw * (int)run.size();
            i = j;
        }
        return x;
    }

    // full dock: x,y = top-left, w,h. Returns nothing.
    void draw_dock(Ctx& u, int x, int y, int w, int h, double now) {
        caret_blink_t = now;
        ui_rect(u, x, y, w, h, 0x10151F);
        ui_frame(u, x, y, w, h, 0x2A3548);
        int cw = 8;                                   // scale-2 glyph advance

        // ---- toolbar
        int ty = y + 5, bh = 16;
        int tx = x + 10;
        struct B { const char* label; int id; uint32_t col; };
        B btns[] = {
            {"NEW PY", 1, 0x1F2B3A}, {"NEW CPP", 2, 0x1F2B3A}, {"OPEN", 3, 0x1F2B3A},
            {"SAVE", 4, 0x1F2B3A},
            {run.running ? "RUNNING" : "RUN  (Ctrl+R)", 5, (uint32_t)(run.running ? 0x3A2430 : 0x143528)},
            {"STOP", 6, 0x351414},
        };
        int hit = -1;
        for (auto& b : btns) {
            int bw = 10 + 8 * (int)std::strlen(b.label);
            ui_rect(u, tx, ty, bw, bh, ui_in(u, tx, ty, bw, bh) ? 0x2A3A52 : b.col);
            ui_frame(u, tx, ty, bw, bh, 0x3A4A66);
            ui_text(u, tx + 5, ty + 3, b.label, 1, b.col == 0x143528 ? 0x3DFF8E : TEXT);
            if (ui_click(u) && ui_in(u, tx, ty, bw, bh)) hit = b.id;
            tx += bw + 6;
        }
        std::string fn = path.empty() ? "untitled" : ide_filename(path);
        ui_text(u, tx + 4, ty + 3, (fn + (dirty ? "   *" : "")).c_str(), 2, GOLD);
        ui_text(u, x + w - 200, ty + 3, cpp_tag(), 1, DIM);

        switch (hit) {
            case 1: new_file(".py"); break;
            case 2: new_file(".cpp"); break;
            case 3: listing = !listing; break;
            case 4: save(); break;
            case 5: do_run(); break;
            case 6: do_stop(); break;
        }

        // ---- file dropdown
        if (listing) {
            auto files = ide_listdir(dir);
            int pw = 240, ph = 14 + 16 * (int)files.size();
            ui_rect(u, x + 130, ty + 18, pw, ph, 0x1B2230);
            ui_frame(u, x + 130, ty + 18, pw, ph, 0x3A4A66);
            for (size_t i = 0; i < files.size(); ++i) {
                int fy = ty + 21 + (int)i * 16;
                bool hov = ui_in(u, x + 130, fy, pw, 15);
                if (hov) ui_rect(u, x + 130, fy, pw, 15, 0x2A3A52);
                ui_text(u, x + 138, fy + 3, files[i].c_str(), 1,
                        hov ? ACCENT : TEXT);
                if (ui_click(u) && hov) { load(ide_join(dir, files[i])); listing = false; }
            }
        }

        // ---- code | console split
        int cy0 = y + 26, ch_h = h - 32;
        int cw_px = (int)(w * (1.0f - console_frac));
        int cons_x = x + cw_px + 4, cons_w = w - cw_px - 8;

        // code area
        int vis = std::max(1, ch_h / 14);
        int vis_cols = std::max(10, (cw_px - 66) / cw);
        int max_scroll = std::max(0, (int)lines.size() - vis + 1);
        scroll_r = clamp_i(scroll_r, 0, max_scroll);
        bool in_code = ui_in(u, x, cy0, cw_px, ch_h);
        if (in_code && ui_click(u)) {
            int r = (ui_my(u) - cy0) / 14;
            cr = scroll_r + r;
            cc = clamp_i((ui_mx(u) - (x + 58)) / cw + scroll_c, 0, 100000);
            clamp_caret();
        }
        // gutter
        ui_rect(u, x, cy0, 48, ch_h, 0x0B0F16);
        for (int row = 0; row < vis && row + scroll_r < (int)lines.size(); ++row) {
            int r = scroll_r + row, yy = cy0 + row * 14;
            if (r == cr) ui_rect(u, x + 48, yy - 2, cw_px - 48, 13, 0x16202E);
            char nb[16]; std::snprintf(nb, sizeof nb, "%d", r + 1);
            ui_text(u, x + 44 - 8 * (int)std::strlen(nb), yy, nb, 1,
                    r == cr ? GOLD : GUT_NUM);
            const std::string& s = lines[r];
            if (!s.empty()) draw_line(u, s, x + 58 - scroll_c * cw, yy, cw);
        }
        // caret
        {
            bool blink = std::fmod(now, 1.0) < 0.62;
            int ry = (cr - scroll_r) * 14;
            int cx = x + 58 + (cc - scroll_c) * cw;
            if (blink && ry >= 0 && ry < vis * 14)
                ui_rect(u, cx, cy0 + ry - 2, 2, 13, 0xFFFFFF);
            if (cx < x + 58) { scroll_c = cc - 20; }
            if (cx > x + cw_px - 10) { scroll_c = cc - (vis_cols - 8); }
            scroll_c = clamp_i(scroll_c, 0, std::max(0, (int)lines[cr].size() - vis_cols + 4));
        }
        // scrollbar for code
        if (max_scroll > 0) {
            int sbh = ch_h, sh2 = std::max(14, sbh * vis / (int)lines.size());
            int sy = cy0 + (sbh - sh2) * scroll_r / std::max(1, max_scroll);
            ui_rect(u, x + cw_px - 6, sy, 4, sh2, 0x3A4A66);
        }

        // console
        ui_rect(u, cons_x, cy0, cons_w, ch_h, 0x0A0E15);
        ui_frame(u, cons_x, cy0, cons_w, ch_h, 0x222C3C);
        ui_text(u, cons_x + 6, cy0 + 3, "CONSOLE", 1, ACCENT);
        if (!run.running) {
            if (run.done) {
                char b[48]; std::snprintf(b, sizeof b, "exit code %d", run.exit_code);
                ui_text(u, cons_x + 70, cy0 + 3, b, 1, run.exit_code == 0 ? 0x3DFF8E : PINK);
                run.done = false;
            }
            ui_text(u, cons_x + 130, cy0 + 3, run.running ? "" : "Ctrl+R to run", 1, GUT_NUM);
        } else {
            ui_text(u, cons_x + 70, cy0 + 3, "running...", 1, GOLD);
        }
        auto log = run.snapshot();
        int lh = 11, rows = std::max(1, (ch_h - 16) / lh);
        int start = std::max(0, (int)log.size() - rows);
        for (int i = 0; i < rows && start + i < (int)log.size(); ++i) {
            const std::string& l = log[start + i];
            uint32_t col = 0x9FB4CC;
            std::string cut = l.size() > 200 ? l.substr(0, 200) : l;
            if (cut.find("error") != std::string::npos || cut.find("Error") != std::string::npos ||
                cut.find("Traceback") != std::string::npos) col = PINK;
            if (!cut.empty() && cut[0] == '>') col = 0x3DFF8E;
            ui_text(u, cons_x + 6, cy0 + 16 + i * lh, cut.c_str(), 1, col);
        }
        if (note[0] || !note.empty()) {
            ui_text(u, cons_x + 6, y + h - 12, note.c_str(), 1, GUT_NUM);
        }
    }

    const char* cpp_tag() { return is_cpp() ? "C++  g++ -std=c++17" : "PYTHON  native_engine API"; }

    void new_file(const char* ext) {
        int k = 1; char nm[40];
        do { std::snprintf(nm, sizeof nm, "script_%d%s", k++, ext); }
        while (ide_exists(ide_join(dir, nm)));
        path = ide_join(dir, nm);
        lines.assign(1, std::string(""));
        cr = cc = scroll_r = scroll_c = 0; dirty = true;
        note = std::string("new ") + nm;
    }
    static int clamp_i(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
    void follow(int vis) {                            // keep caret in view
        if (cr < scroll_r) scroll_r = cr;
        if (cr >= scroll_r + vis) scroll_r = cr - vis + 1;
        if (scroll_r < 0) scroll_r = 0;
    }

    // colors (kept local so this header stays stand-alone)
    enum : uint32_t {
        TEXT = 0xC9D4E4, DIM_DEF = 0x6B7A93, ACCENT = 0x35E7FF,
        PINK = 0xFF4FB2, GOLD = 0xFFC93D, GREEN_DEF = 0x3DFF8E,
        CODE_KW = 0x35E7FF, CODE_STR = 0xFF9BCE, CODE_CMT = 0x50607A,
        CODE_NUM = 0xFFC93D, CODE_PP = 0x3DFF8E
    };
};

} // namespace ed

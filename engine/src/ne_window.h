// ============================================================================
//  Native Engine — ne_window.h
//  Native desktop window: Win32 window + GDI presentation + keyboard/mouse,
//  or a headless framebuffer when there is no window system (tests, CI).
//  Depends on nothing beyond the OS — same "no std::thread" philosophy as
//  ne_threads.h so it links on any MinGW build.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <chrono>

namespace ne {

class Window {
public:
    int width = 0, height = 0;
    float mx = 0, my = 0;         // mouse position (client pixels)
    float mdx = 0, mdy = 0;       // mouse delta since last pump
    bool lmb = false, rmb = false;
    int key_down[256] = {};       // held state
    int key_pressed[256] = {};    // edge events (cleared each pump)
    bool alive = false;

    double now() const {
        auto d = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double>(d).count();
    }

    bool create(const char* title, int w, int h);
    void present(const uint8_t* rgba);   // push framebuffer to screen
    bool pump();                          // handle messages; false when closing
    void set_title(const char* t);

    // headless test hooks (no-ops on real windows)
    void inject_key(int vk, bool down) {
        if (down) { if (!key_down[vk]) key_pressed[vk] = 1; key_down[vk] = 1; }
        else key_down[vk] = 0;
    }

private:
    void* hwnd_ = nullptr;
    void* devmem_ = nullptr;      // top-down 32-bit DIB pixel memory
    void* gdiobj_ = nullptr;      // memory DC handle
    float lastmx_ = -1, lastmy_ = -1;

#if defined(_WIN32)
    static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
#endif
};

// --------------------------------------------------------------------------
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

inline LRESULT CALLBACK Window::wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Window* w = (Window*)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (!w) return DefWindowProcA(h, msg, wp, lp);
    switch (msg) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (wp < 256) { if (!w->key_down[wp]) w->key_pressed[wp] = 1; w->key_down[wp] = 1; }
        return 0;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (wp < 256) w->key_down[wp] = 0;
        if (wp == VK_ESCAPE && w->hwnd_) DestroyWindow(h);
        return 0;
    case WM_MOUSEMOVE:
        w->mx = (float)(short)LOWORD(lp); w->my = (float)(short)HIWORD(lp);
        return 0;
    case WM_LBUTTONDOWN: w->lmb = true; return 0;
    case WM_LBUTTONUP:   w->lmb = false; return 0;
    case WM_RBUTTONDOWN: w->rmb = true; return 0;
    case WM_RBUTTONUP:   w->rmb = false; return 0;
    case WM_CLOSE: case WM_DESTROY:
        w->alive = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

inline bool Window::create(const char* title, int w, int h) {
    static bool registered = false;
    HINSTANCE hi = GetModuleHandleA(nullptr);
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = &Window::wndproc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "NativeEngineWindow";
        RegisterClassA(&wc);
        registered = true;
    }
    width = w; height = h;
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, style, FALSE);
    HWND wnd = CreateWindowA("NativeEngineWindow", title, style,
                             CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                             rc.bottom - rc.top, nullptr, nullptr, hi, nullptr);
    if (!wnd) return false;
    SetWindowLongPtrA(wnd, GWLP_USERDATA, (LONG_PTR)(intptr_t)this);
    hwnd_ = wnd; alive = true;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   // top-down rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC screen = GetDC(wnd);
    HDC memdc = CreateCompatibleDC(screen);
    ReleaseDC(wnd, screen);
    HBITMAP bmp = CreateDIBSection(memdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bits || !bmp) return false;
    SelectObject(memdc, bmp);
    devmem_ = bits;
    gdiobj_ = (void*)(intptr_t)memdc;
    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);
    return true;
}

inline void Window::present(const uint8_t* rgba) {
    if (!hwnd_ || !devmem_) return;
    std::memcpy(devmem_, rgba, (size_t)width * height * 4);
    HDC screen = GetDC((HWND)hwnd_);
    BitBlt(screen, 0, 0, width, height, (HDC)(intptr_t)gdiobj_, 0, 0, SRCCOPY);
    ReleaseDC((HWND)hwnd_, screen);
}

inline bool Window::pump() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { alive = false; return false; }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    mdx = mx - lastmx_; mdy = my - lastmy_;
    if (lastmx_ < 0) mdx = mdy = 0;
    lastmx_ = mx; lastmy_ = my;
    std::memset(key_pressed, 0, sizeof(key_pressed));
    return alive;
}

inline void Window::set_title(const char* t) { if (hwnd_) SetWindowTextA((HWND)hwnd_, t); }

#else  // ---- headless (Linux/macOS: tests + CI) ------------------------------

inline bool Window::create(const char* /*title*/, int w, int h) {
    width = w; height = h; alive = true;
    return true;
}
inline void Window::present(const uint8_t*) {}
inline bool Window::pump() {
    mdx = mx - lastmx_; mdy = my - lastmy_;
    if (lastmx_ < 0) mdx = mdy = 0;
    lastmx_ = mx; lastmy_ = my;
    std::memset(key_pressed, 0, sizeof(key_pressed));
    return alive;
}
inline void Window::set_title(const char*) {}

#endif

} // namespace ne

// ============================================================================
//  Native Engine — ne_threads.h
//  OS-native threading shim so the engine links on EVERY toolchain, including
//  MinGW builds with the "win32 threads model" where std::thread's
//  _M_start_thread is unavailable (winpthreads missing) and linking fails.
//
//    Windows : _beginthreadex (msvcrt) + WaitForSingleObject (kernel32)
//    others  : std::thread
//
//  Also provides a header-only spinlock (std::atomic_flag) replacing
//  std::mutex/lock_guard, which on some exotic MinGW builds also drag in
//  winpthreads symbols.
// ============================================================================
#pragma once

#include <atomic>
#include <cstdint>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <process.h>  // _beginthreadex / _endthreadex
    #define NE_THREAD_ENTRY unsigned __stdcall
#else
    #include <thread>
    #if defined(__unix__) || defined(__APPLE__)
        #include <unistd.h>
    #endif
    #define NE_THREAD_ENTRY void
#endif

namespace ne {

#if defined(_WIN32)

using thread_handle = uintptr_t;  // _beginthreadex handle (0 on failure)

inline thread_handle spawn_thread(NE_THREAD_ENTRY (*entry)(void*), void* arg) {
    return _beginthreadex(nullptr, 0, entry, arg, 0, nullptr);
}
inline void join_thread(thread_handle h) {
    if (!h) return;
    HANDLE hs = reinterpret_cast<HANDLE>(h);
    WaitForSingleObject(hs, INFINITE);
    CloseHandle(hs);
}
inline unsigned hardware_threads() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1u;
}

#else  // POSIX ---------------------------------------------------------------

using thread_handle = std::thread;

inline thread_handle spawn_thread(void (*entry)(void*), void* arg) {
    return std::thread(entry, arg);
}
inline void join_thread(thread_handle& h) {
    if (h.joinable()) h.join();
}
inline unsigned hardware_threads() {
    long n = 0;
#if defined(_SC_NPROCESSORS_ONLN)
    n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (n > 0) return (unsigned)n;
    unsigned hw = std::thread::hardware_concurrency();
    return hw ? hw : 1u;
}

#endif

// ---- header-only spinlock (no libc threading dependency) -------------------
class SpinLock {
public:
    void lock()   { while (m_flag.test_and_set(std::memory_order_acquire)) {} }
    void unlock() { m_flag.clear(std::memory_order_release); }
private:
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

struct SpinGuard {
    explicit SpinGuard(SpinLock& l) : m_l(l) { m_l.lock(); }
    ~SpinGuard() { m_l.unlock(); }
    SpinGuard(const SpinGuard&) = delete;
    SpinGuard& operator=(const SpinGuard&) = delete;
private:
    SpinLock& m_l;
};

} // namespace ne

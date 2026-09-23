// ============================================================================
//  Native Engine — ne_app.cpp
//  Desktop application entry point: opens a native window and runs the
//  NEON RUNNER game at 60 fps (software rasterizer), with F1 to switch to
//  progressive path-traced "RT mode".
//
//  Windows build (the shipped .exe):
//    g++ -std=c++17 -O3 -ffast-math -mwindows -static \
//        -Iengine/src engine/src/ne_app.cpp -o NativeEngine.exe
//
//  Headless self-test (Linux/CI):
//    g++ -std=c++17 -O2 -Iengine/src engine/src/ne_app.cpp -o ne_app
//    ./ne_app --headless 240 --shot game_test.png
// ============================================================================
#include "ne_game.h"
#include "ne_renderer.h"   // Renderer (RT mode) + encode_png for headless shots
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

using namespace ne;

struct App {
    Window win;
    Renderer rt{1, 1};
    Game game{rt.scene};
    int headless_frames = -1;
    std::string shot;
    int w = 960, h = 540;
    int frame = 0;

    void apply_args(int argc, char** argv) {
        for (int i = 1; i < argc - 1; ++i) {
            if (!strcmp(argv[i], "--headless")) headless_frames = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--shot")) shot = argv[++i];
            else if (!strcmp(argv[i], "--w")) w = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--h")) h = atoi(argv[++i]);
        }
    }

    int run(int argc = 0, char** argv = nullptr) {
        if (argc && argv) apply_args(argc, argv);
        if (!win.create("Native Engine - NEON RUNNER", w, h)) {
            fprintf(stderr, "window creation failed\n");
            return 2;
        }
        rt.resize(w, h);
        rt.streaming = true;
        game.win = &win;
        game.rt = headless_frames < 0 ? &rt : nullptr;   // RT hotkey off in CI
        game.raster.resize(w, h);
        game.build();
        printf("Native Engine — NEON RUNNER | %dx%d | threads: %u\n", w, h,
               ne::hardware_threads());

        double prev = win.now();
        double acc = 0;
        const double step = 1.0 / 120.0;
        while (win.alive) {
            double t0 = win.now();
            if (!win.pump()) break;

            // scripted input for the headless test: run north-west for a while
            if (headless_frames >= 0) {
                win.inject_key('W', frame > 5 && frame < 400);
                win.inject_key(0x20, frame == 40);       // one jump
                if (frame > 60 && frame < 200) win.inject_key('D', true);
                if (frame == 200) win.inject_key('D', false);
            }

            double t = win.now();
            float dt = (float)(t - prev); prev = t;
            if (dt > 0.1f) dt = 0.1f;
            game.fps += (1.0f / (dt + 1e-4f) - game.fps) * 0.08f;
            acc += dt;
            int n = 0;
            while (acc >= step && n < 6) { game.update((float)step); acc -= step; ++n; }

            game.render_frame();
            ++frame;

            if (headless_frames >= 0) {
                if (frame == (headless_frames > 90 ? 90 : headless_frames) && !shot.empty()) {
                    std::string mid = shot;
                    size_t dot = mid.find_last_of('.');
                    if (dot != std::string::npos) mid.insert(dot, "_mid");
                    auto png = encode_png(game.raster.rgba.data(), w, h);
                    FILE* f = fopen(mid.c_str(), "wb");
                    if (f) { fwrite(png.data(), 1, png.size(), f); fclose(f); }
                    printf("wrote %s | score=%d lives=%d (alive check)\n", mid.c_str(), game.score, game.lives);
                }
                if (frame == headless_frames) {
                    if (!shot.empty()) {
                        auto png = encode_png(game.raster.rgba.data(), w, h);
                        FILE* f = fopen(shot.c_str(), "wb");
                        if (f) { fwrite(png.data(), 1, png.size(), f); fclose(f); }
                        printf("wrote %s (%zu bytes) | score=%d lives=%d\n",
                               shot.c_str(), png.size(), game.score, game.lives);
                    }
                    printf("headless done: frames=%d score=%d lives=%d fps=%.0f elapsed=%.1f\n",
                           frame, game.score, game.lives, game.fps, game.elapsed);
                    return 0;
                }
            } else {
                if (frame % 45 == 1) {
                    char buf[128];
                    snprintf(buf, sizeof buf, "Native Engine - NEON RUNNER | crystals %d/%d | fps %.0f",
                             game.score, game.n_crystals, game.fps);
                    win.set_title(buf);
                }
                double spent = win.now() - t0;
                const double target = 1.0 / 60.0;
                if (spent < target) {
#ifdef _WIN32
                    Sleep((DWORD)((target - spent) * 1000.0 + 0.5));
#else
                    long ns = (long)((target - spent) * 1e9);
                    struct timespec ts{ns / 1000000000L, ns % 1000000000L};
                    nanosleep(&ts, nullptr);
#endif
                }
            }
        }
        return 0;
    }
};

extern "C" int ne_game_app_main(int w, int h, int frame_limit, const char* shot) {
    App app;
    app.w = w > 0 ? w : 1024;
    app.h = h > 0 ? h : 576;
    app.headless_frames = frame_limit;
    app.shot = shot ? shot : "";
    return app.run();
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return ne_game_app_main(1024, 576, 0, nullptr);
}
#else
#include <ctime>
int main(int argc, char** argv) {
    App app;
    return app.run(argc, argv);
}
#endif

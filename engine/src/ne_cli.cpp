// ============================================================================
//  Native Engine — ne_cli.cpp
//  Standalone command-line renderer:
//      ne_cli --w 1280 --h 720 --spp 128 --angle 25 --out render.png
//  Builds the signature showcase scene and renders it entirely in C++,
//  no Python required.
// ============================================================================
#include "ne_demo.h"
#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    int w = 1280, h = 720, spp = 64, bounces = 5;
    float angle = 0.0f;
    std::string out = "render.png";
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() { return (i + 1 < argc) ? argv[++i] : ""; };
        if (!strcmp(argv[i], "--w")) w = atoi(next());
        else if (!strcmp(argv[i], "--h")) h = atoi(next());
        else if (!strcmp(argv[i], "--spp")) spp = atoi(next());
        else if (!strcmp(argv[i], "--bounces")) bounces = atoi(next());
        else if (!strcmp(argv[i], "--angle")) angle = atof(next());
        else if (!strcmp(argv[i], "--out")) out = next();
        else { fprintf(stderr, "unknown flag %s\n", argv[i]); return 1; }
    }

    printf("Native Engine CLI — %dx%d, %d spp, %d bounces\n", w, h, spp, bounces);
    NEEngine* e = ne_create(w, h);
    if (!e) { fprintf(stderr, "engine creation failed\n"); return 1; }
    printf("version : %s\n", ne_version());
    printf("threads : %d\n", ne_stat_threads(e));

    ne::build_showcase_scene(e);
    ne::set_showcase_camera(e, angle);
    ne_set_max_bounces(e, bounces);
    ne_set_streaming(e, 0); // accumulate all spp into one clean frame
    printf("objects : %d\n", ne_stat_objects(e));

    double ms = 0;
    for (int pass = 0; pass < spp; pass += 16) {
        int batch = std::min(16, spp - pass);
        ne_render(e, batch);
        printf("\r  pass %4d/%d  (%.1f Mrays/s)   ", pass + batch, spp, ne_stat_mrays(e));
        fflush(stdout);
    }
    ms = ne_stat_ms(e);
    printf("\n  last pass: %.1f ms\n", ms);

    int len = 0;
    const unsigned char* png = ne_frame_png(e, &len);
    FILE* f = fopen(out.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
    fwrite(png, 1, len, f);
    fclose(f);
    printf("wrote %s (%d bytes)\n", out.c_str(), len);
    ne_destroy(e);
    return 0;
}

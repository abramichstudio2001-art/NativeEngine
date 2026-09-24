// ============================================================================
//  Native Engine — ne_editor.cpp
//  The desktop application: an RTX editor UI around the streaming viewport.
//
//    ┌──────────────────────────────────────────────────────────────┐
//    │  NATIVE ENGINE · RTX EDITOR        buttons: snapshot save…   │
//    ├───────────┬──────────────────────────────────┬──────────────┤
//    │ OUTLINER  │        RT VIEWPORT               │  INSPECTOR   │
//    │ objects   │  progressive path-traced tiles   │  transform   │
//    │ +add/-del │  streaming + denoised, 60fps UI  │  material    │
//    │           │  LMB pick · RMB orbit · wheel    │  sky / sun   │
//    ├───────────┴──────────────────────────────────┴──────────────┤
//    │  fps · converged spp · Mrays/s · pass status                 │
//    └──────────────────────────────────────────────────────────────┘
//
//  Rendering model ("RTX that feels fast"): the path tracer never blocks the
//  UI. Each frame renders whatever 32px tiles fit in a few milliseconds
//  (round-robin), tonemaps the partial state, edge-denoises it, and upscales
//  — so interaction shows lit, GI-correct motion immediately while quality
//  converges underneath. Scene edits re-queue the stream.
// ============================================================================
#include "ne_renderer.h"
#include "ne_window.h"
#include "ne_ide.h"
#include "ne_raster.h"     // for raster_glyph (3x5 bitmap font)
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>

using namespace ne;

// ---------------------------------------------------------------------------
//  Tiny canvas + immediate-mode widgets drawn on an RGBA8 buffer
// ---------------------------------------------------------------------------
namespace ed {

struct Ctx {
    uint8_t* p = nullptr;
    int W = 0, H = 0;
    int mx = 0, my = 0;
    bool down = false, click = false;
    int cy0 = 0, cy1 = 1 << 30;      // clip rows (for scrollable panels)

    void clipY(int y0, int y1) { cy0 = y0; cy1 = y1; }

    void px(int x, int y, uint32_t c, float a = 1.0f) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        if (y < cy0 || y >= cy1) return;
        size_t j = ((size_t)y * W + x) * 4;
        uint8_t cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
        if (a >= 0.999f) { p[j] = cr; p[j + 1] = cg; p[j + 2] = cb; return; }
        p[j]     = (uint8_t)(p[j]     * (1 - a) + cr * a);
        p[j + 1] = (uint8_t)(p[j + 1] * (1 - a) + cg * a);
        p[j + 2] = (uint8_t)(p[j + 2] * (1 - a) + cb * a);
    }
    void rect(int x, int y, int w, int h, uint32_t c, float a = 1.0f) {
        for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i) px(i, j, c, a);
    }
    void frame_rect(int x, int y, int w, int h, uint32_t c) {
        for (int i = x; i < x + w; ++i) { px(i, y, c); px(i, y + h - 1, c); }
        for (int j = y; j < y + h; ++j) { px(x, j, c); px(x + w - 1, j, c); }
    }
    void text(int x, int y, const char* s, int scale, uint32_t c) {
        for (; *s; ++s) {
            if (*s == ' ') { x += 4 * scale; continue; }
            const char* g = raster_glyph(*s);
            for (int row = 0; row < 5; ++row)
                for (int col = 0; col < 3; ++col)
                    if (g[row * 3 + col] == '1')
                        for (int sy = 0; sy < scale; ++sy)
                            for (int sx = 0; sx < scale; ++sx)
                                px(x + col * scale + sx, y + row * scale + sy, c);
            x += 4 * scale;
        }
    }
    bool in(int x, int y, int w, int h) const { return mx >= x && mx < x + w && my >= y && my < y + h; }
};

inline void ui_text(Ctx& u, int x, int y, const char* s, int scale, uint32_t c) { u.text(x, y, s, scale, c); }
inline void ui_rect(Ctx& u, int x, int y, int w, int h, uint32_t c, float a) { u.rect(x, y, w, h, c, a); }
inline void ui_frame(Ctx& u, int x, int y, int w, int h, uint32_t c) { u.frame_rect(x, y, w, h, c); }
inline bool ui_in(Ctx& u, int x, int y, int w, int h) { return u.in(x, y, w, h); }
inline bool ui_click(Ctx& u) { return u.click; }
inline int  ui_mx(Ctx& u) { return u.mx; }
inline int  ui_my(Ctx& u) { return u.my; }




struct Editor {
    Window win;
    Renderer rt{2, 2};

    // viewport / layout
    int W = 0, H = 0;
    int vp_x = 0, vp_y = 0, vp_w = 0, vp_h = 0;    // viewport rect (window px)
    int rw = 0, rh = 0;                              // RT internal res
    float view_scale = 0.5f;
    int budget_ms = 5;
    uint32_t target_passes = 8;
    int stream_spp = 1;
    bool denoise_on = true;

    // camera orbit
    float cam_az = 22.0f, cam_el = 17.0f, cam_dist = 6.6f;
    Vec3 cam_tgt{0, 0.95f, -0.2f};
    bool autorotate = false;

    // selection & misc
    int sel = -1;
    int rmb_was = 0;
    float insp_scroll = 0.0f;
    float fps = 60.0f, mrays = 0.0f;
    double last_t = 0;
    std::string status = "READY";
    float status_t = 0;
    uint32_t toast_col = GOLD;
    int frame_no = 0;
    bool snap_pending = false;
    std::string snap_path;

    std::vector<uint8_t> disp, tmp;                  // display RGBA

    // built-in IDE + modal help
    Ide ide;
    bool ide_inited = false;
    int dock_y = 0, dock_h = 0;
    bool help_open = false;
    bool ide_typed = false;
    const char* tip = nullptr;                        // hover tooltip (per-frame)

    // ------------------------------------------------------------- scene ----
    int add_mat(Vec3 al, float met, float rough, Vec3 em,
                float ior, float trans, float checker) {
        Material m;
        m.albedo = al; m.metallic = met; m.roughness = rough;
        m.emissive = em; m.ior = ior; m.transmission = trans;
        m.checker = checker;
        return rt.scene.add_material(m);
    }
    void default_scene() {
        Scene& s = rt.scene;
        s.materials.clear(); s.objects.clear(); s.lights.clear();
        Vec3 wht(0.85f, 0.83f, 0.80f), glass(0.97f, 0.99f, 1.0f), z(0.0f);
        Vec3 gold(1.0f, 0.71f, 0.29f), red(0.78f, 0.12f, 0.10f);
        Vec3 mag(1.0f, 0.16f, 0.95f), cya(0.11f, 0.72f, 0.95f);
        int m_floor = add_mat(wht, 0, 0.32f, z, 1.5f, 0, 1.25f);
        int m_glass = add_mat(glass, 0, 0.03f, z, 1.52f, 1.0f, 0);
        int m_gold  = add_mat(gold, 1.0f, 0.14f, z, 1.5f, 0, 0);
        int m_red   = add_mat(red, 0, 0.85f, z, 1.5f, 0, 0);
        int mm1 = add_mat(mag, 0, 1.0f, Vec3(8.5f, 1.4f, 8.0f), 1.5f, 0, 0);
        int mm2 = add_mat(cya, 0, 1.0f, Vec3(1.1f, 7.5f, 9.5f), 1.5f, 0, 0);

        { Object o; o.type = ObjType::Plane; o.mat = m_floor; o.normal = Vec3(0, 1, 0); o.d = 0; s.add_object(o); }
        { Object o; o.type = ObjType::Sphere; o.mat = m_glass; o.pos = {0, 1.05f, -0.2f}; o.radius = 1.0f; s.add_object(o); }
        { Object o; o.type = ObjType::Sphere; o.mat = m_gold;  o.pos = {-2.6f, 0.78f, 1.2f}; o.radius = 0.78f; s.add_object(o); }
        { Object o; o.type = ObjType::Sphere; o.mat = m_red;   o.pos = {2.5f, 0.68f, 1.0f}; o.radius = 0.68f; s.add_object(o); }
        { Object o; o.type = ObjType::Quad; o.mat = mm1;
          o.q_p0 = {-5.6f, 0.4f, -2.0f}; o.q_u = {0, 2.6f, 0}; o.q_v = {0, 0, 4.0f};
          o.q_normal = normalize(cross(o.q_u, o.q_v)); o.q_area = length(cross(o.q_u, o.q_v)); s.add_object(o); }
        { Object o; o.type = ObjType::Quad; o.mat = mm2;
          o.q_p0 = {3.2f, 0.4f, -5.6f}; o.q_u = {3.2f, 0, 0}; o.q_v = {0, 2.6f, 0};
          o.q_normal = normalize(cross(o.q_u, o.q_v)); o.q_area = length(cross(o.q_u, o.q_v)); s.add_object(o); }

        s.sky.top = Vec3(0.25f, 0.45f, 0.85f);
        s.sky.horizon = Vec3(0.9f, 0.72f, 0.58f);
        s.sky.sun_dir = normalize(Vec3(0.42f, 0.55f, 0.28f));
        s.sky.sun_color = Vec3(1.0f, 0.9f, 0.78f);
        s.sky.sun_intensity = 4.5f;
        s.sky.sun_cos_angle = std::cos(1.5f * NE_PI / 180.0f);
        s.sky.intensity = 1.0f;
        s.max_bounces = 5;
        rt.exposure = 1.15f;
        sel = -1;
    }

    void apply_cam() {
        float azr = cam_az * NE_PI / 180.0f, elr = cam_el * NE_PI / 180.0f;
        Vec3 eye = cam_tgt + Vec3(std::sin(azr) * std::cos(elr), std::sin(elr),
                                   std::cos(azr) * std::cos(elr)) * cam_dist;
        Camera& c = rt.scene.camera;
        c.pos = eye; c.target = cam_tgt; c.fov_y_deg = 50.0f;
        c.aperture = 0.0f; c.focus_dist = cam_dist;
    }

    void resize_rt() {
        rw = std::max(96, (int)((float)vp_w * view_scale) & ~31);
        rh = std::max(64, (int)((float)vp_h * view_scale) & ~31);
        if (rw * rh > 0) { rt.resize(rw, rh); tmp.resize((size_t)W * H * 4); }
    }
    void touch() { rt.begin_stream(stream_spp, target_passes); }

    // ---------------------------------------------------------- inspector ---
    float slider(Ctx& u, const char* label, float x, float y, float w,
                 float v, float lo, float hi, const char* fmt = "%.2f",
                 uint32_t col = ACCENT) {
        char b[64]; std::snprintf(b, sizeof b, "%s  ", label);
        char b2[64]; std::snprintf(b2, sizeof b2, fmt, v);
        std::strncat(b, b2, sizeof b - std::strlen(b) - 1);
        u.text((int)x, (int)y - 11, b, 1, DIM);
        float t = (v - lo) / (hi - lo + 1e-9f);
        u.rect((int)x, (int)y, (int)w, 6, TRACK);
        u.rect((int)x, (int)y, (int)(t * w), 6, col, 0.85f);
        u.rect((int)(x + t * w) - 2, (int)y - 2, 5, 10, 0xFFFFFF);
        if (u.down && u.in((int)x - 4, (int)y - 9, (int)w + 8, 20)) {
            t = clampf((u.mx - x) / w, 0.0f, 1.0f);
            v = lo + t * (hi - lo);
            // keep pointer grab: next frame re-evaluates at new v — stable
        }
        return v;
    }
    bool button(Ctx& u, const char* s, float x, float y, float w, float h, uint32_t col = PANEL2) {
        bool hov = u.in((int)x, (int)y, (int)w, (int)h);
        if (hov) tip = s;
        u.rect((int)x, (int)y, (int)w, (int)h, hov ? ACCENT : col, hov ? 0.28f : 1.0f);
        u.frame_rect((int)x, (int)y, (int)w, (int)h, 0x2A3548);
        u.text((int)(x + (w - std::strlen(s) * 8) / 2), (int)(y + h / 2 - 3), s, 2,
               hov ? 0xFFFFFF : TEXT);
        return hov && u.click;
    }
    bool toggle(Ctx& u, const char* s, float x, float y, bool& v) {
        u.rect((int)x, (int)y, 10, 10, v ? GREEN : TRACK, v ? 0.9f : 1.0f);
        u.frame_rect((int)x, (int)y, 10, 10, 0x2A3548);
        u.text((int)x + 15, (int)y + 2, s, 2, TEXT);
        if (u.in((int)x, (int)y, 120, 12) && u.click) { v = !v; return true; }
        return false;
    }
    const char* kind_name(const Object& o) {
        switch (o.type) {
        case ObjType::Sphere: return "SPHERE"; case ObjType::Box: return "BOX";
        case ObjType::Quad: return "QUAD"; case ObjType::Plane: return "PLANE";
        case ObjType::Mesh: return "MESH"; }
        return "?";
    }
    void add_object(ObjType t) {
        Object o; o.type = t;
        o.mat = add_mat(Vec3(0.8f, 0.8f, 0.85f), 0, 0.4f, Vec3(0.0f), 1.5f, 0, 0);
        o.pos = {cam_tgt.x, 1.0f, cam_tgt.z};
        if (t == ObjType::Sphere) o.radius = 0.6f;
        if (t == ObjType::Box) o.half = {0.5f, 0.5f, 0.5f};
        if (t == ObjType::Quad) {
            o.q_p0 = {o.pos.x - 1, 0.4f, o.pos.z - 1};
            o.q_u = {2, 0, 0}; o.q_v = {0, 2.6f, 0};
            o.q_normal = normalize(cross(o.q_u, o.q_v)); o.q_area = length(cross(o.q_u, o.q_v));
        }
        if (t == ObjType::Plane) { o.normal = Vec3(0, 1, 0); rt.scene.materials[o.mat].checker = 1.0f; }
        rt.scene.add_object(o);
        sel = (int)rt.scene.objects.size() - 1;
        touch();
    }
    void hide_selected() {
        if (sel < 0 || sel >= (int)rt.scene.objects.size()) return;
        Object& o = rt.scene.objects[sel];
        o.pos = Vec3(0, -100000.0f, 0); o.radius = 0; o.half = Vec3(0);
        o.q_p0 = Vec3(0, -100000, 0); o.d = 100000.0f;
        o.local_verts.clear(); o.indices.clear();
        sel = -1; touch();
    }
    void pick(int px, int py) {
        if (rt.view_w() <= 0) return;
        float lx = (px - vp_x) / (vp_w / (float)rt.view_w());
        float ly = (py - vp_y) / (vp_h / (float)rt.view_h());
        if (lx < 0 || ly < 0 || lx > rt.view_w() || ly > rt.view_h()) return;
        RNG dummy; dummy.seed(1, 2);
        Vec3 o_, d_;
        rt.scene.camera.make_ray(lx, ly, rt.view_w(), rt.view_h(), dummy, o_, d_);
        Hit hit; hit.t = 1e30f;
        if (rt.scene.intersect(o_, d_, hit) && hit.obj >= 0) sel = hit.obj;
        else sel = -1;
    }

    // --------------------------------------------------- snapshot / files ---
    void snapshot(const char* path) {
        float s0 = view_scale; view_scale = 1.0f; resize_rt();
        touch();
        while (rt.stream_step(10000000)) {}       // converge at full res (blocking)
        rt.present_stream();
        std::vector<uint8_t> png; rt.get_png(png);
        FILE* f = fopen(path, "wb");
        if (f) { fwrite(png.data(), 1, png.size(), f); fclose(f); }
        char b[96]; std::snprintf(b, sizeof b, "SAVED %s (%zuKB)", path, png.size() / 1024);
        status = b; status_t = 3.0f;
        view_scale = s0; resize_rt(); touch();
    }
    void save_scene(const char* path) {
        FILE* f = fopen(path, "wb"); if (!f) { status = "SAVE FAILED"; status_t = 2; return; }
        Scene& s = rt.scene;
        fprintf(f, "NEENGINE_SCENE 1\n");
        for (auto& m : s.materials)
            fprintf(f, "mat %.5g %.5g %.5g %.3g %.3g %.3g %.3g %.3g %.5g %.5g %.5g\n",
                    m.albedo.x, m.albedo.y, m.albedo.z, m.metallic, m.roughness,
                    m.ior, m.transmission, m.checker, m.emissive.x, m.emissive.y, m.emissive.z);
        for (auto& o : s.objects) {
            if (o.type == ObjType::Sphere)
                fprintf(f, "obj sphere %d %.4g %.4g %.4g %.4g\n", o.mat, o.pos.x, o.pos.y, o.pos.z, o.radius);
            else if (o.type == ObjType::Box)
                fprintf(f, "obj box %d %.4g %.4g %.4g %.4g %.4g %.4g %.3g\n", o.mat, o.pos.x, o.pos.y, o.pos.z,
                        o.half.x, o.half.y, o.half.z, o.rot_y);
            else if (o.type == ObjType::Quad)
                fprintf(f, "obj quad %d %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g\n", o.mat,
                        o.q_p0.x, o.q_p0.y, o.q_p0.z, o.q_u.x, o.q_u.y, o.q_u.z, o.q_v.x, o.q_v.y, o.q_v.z);
            else if (o.type == ObjType::Plane)
                fprintf(f, "obj plane %d %.4g\n", o.mat, o.d);
        }
        fprintf(f, "cam %.3f %.3f %.3f %.3f %.3f %.3f\n", cam_az, cam_el, cam_dist, cam_tgt.x, cam_tgt.y, cam_tgt.z);
        fprintf(f, "sun %.3f %.3f %.3g %.3g\n", sun_az(), sun_el(), s.sky.sun_intensity,
                std::acos(clampf(s.sky.sun_cos_angle, -1, 1)) * 180.0 / NE_PI * 2);
        fprintf(f, "sky %.4g %.4g %.4g %.4g %.4g %.4g %.3g %.3g\n",
                s.sky.top.x, s.sky.top.y, s.sky.top.z, s.sky.horizon.x, s.sky.horizon.y, s.sky.horizon.z,
                s.sky.intensity, rt.exposure);
        fprintf(f, "render %d %d %.2f %u %d\n", rt.scene.max_bounces, budget_ms, view_scale,
                target_passes, denoise_on ? 1 : 0);
        fclose(f);
        status = "SCENE SAVED"; status_t = 2;
    }
    void load_scene(const char* path) {
        FILE* f = fopen(path, "rb"); if (!f) { status = "LOAD FAILED"; status_t = 2; return; }
        char line[256];
        Scene& s = rt.scene;
        s.materials.clear(); s.objects.clear(); s.lights.clear();
        while (fgets(line, sizeof line, f)) {
            char t[16];
            if (sscanf(line, "%15s", t) != 1) continue;
            if (!strcmp(t, "mat")) {
                Material m;
                sscanf(line + 4, "%f %f %f %f %f %f %f %f %f %f %f",
                       &m.albedo.x, &m.albedo.y, &m.albedo.z, &m.metallic, &m.roughness,
                       &m.ior, &m.transmission, &m.checker, &m.emissive.x, &m.emissive.y, &m.emissive.z);
                s.materials.push_back(m);
            } else if (!strcmp(t, "obj")) {
                char kind[16]; sscanf(line + 4, "%15s", kind);
                Object o; float v[10]; int mat;
                if (!strcmp(kind, "sphere") && sscanf(line, "obj sphere %d %f %f %f %f", &mat, &v[0], &v[1], &v[2], &v[3]) == 5) {
                    o.type = ObjType::Sphere; o.mat = mat; o.pos = {v[0], v[1], v[2]}; o.radius = v[3];
                } else if (!strcmp(kind, "box") && sscanf(line, "obj box %d %f %f %f %f %f %f %f", &mat, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) == 8) {
                    o.type = ObjType::Box; o.mat = mat; o.pos = {v[0], v[1], v[2]}; o.half = {v[3], v[4], v[5]}; o.rot_y = v[6];
                } else if (!strcmp(kind, "quad") && sscanf(line, "obj quad %d %f %f %f %f %f %f %f %f %f", &mat, v, v + 1, v + 2, v + 3, v + 4, v + 5, v + 6, v + 7, v + 8) == 10) {
                    o.type = ObjType::Quad; o.mat = mat;
                    o.q_p0 = {v[0], v[1], v[2]}; o.q_u = {v[3], v[4], v[5]}; o.q_v = {v[6], v[7], v[8]};
                    o.q_normal = normalize(cross(o.q_u, o.q_v)); o.q_area = length(cross(o.q_u, o.q_v));
                } else if (!strcmp(kind, "plane") && sscanf(line, "obj plane %d %f", &mat, &v[0]) == 2) {
                    o.type = ObjType::Plane; o.mat = mat; o.normal = Vec3(0, 1, 0); o.d = v[0];
                } else continue;
                s.add_object(o);
            } else if (!strcmp(t, "cam")) {
                sscanf(line + 4, "%f %f %f %f %f %f", &cam_az, &cam_el, &cam_dist, &cam_tgt.x, &cam_tgt.y, &cam_tgt.z);
            } else if (!strcmp(t, "sun")) {
                float az, el, inten, rad;
                if (sscanf(line + 4, "%f %f %f %f", &az, &el, &inten, &rad) == 4) {
                    set_sun(az, el); s.sky.sun_intensity = inten;
                    s.sky.sun_cos_angle = std::cos(std::max(0.1f, rad * 0.5f) * NE_PI / 180.0f);
                }
            } else if (!strcmp(t, "sky")) {
                sscanf(line + 4, "%f %f %f %f %f %f %f %f", &s.sky.top.x, &s.sky.top.y, &s.sky.top.z,
                       &s.sky.horizon.x, &s.sky.horizon.y, &s.sky.horizon.z, &s.sky.intensity, &rt.exposure);
            } else if (!strcmp(t, "render")) {
                int dn; sscanf(line + 7, "%d %d %f %u %d", &s.max_bounces, &budget_ms, &view_scale, &target_passes, &dn);
                denoise_on = dn != 0;
            }
        }
        fclose(f);
        sel = -1; apply_layout(); apply_cam(); resize_rt(); touch();
        status = "SCENE LOADED"; status_t = 2;
    }

    float sun_az() const {
        return std::atan2(rt.scene.sky.sun_dir.x, rt.scene.sky.sun_dir.z) * 180.0f / NE_PI;
    }
    float sun_el() const {
        return std::asin(clampf(rt.scene.sky.sun_dir.y, -1, 1)) * 180.0f / NE_PI;
    }
    void set_sun(float az, float el) {
        float a = az * NE_PI / 180.0f, e = el * NE_PI / 180.0f;
        rt.scene.sky.sun_dir = normalize(Vec3(std::sin(a) * std::cos(e), std::sin(e), std::cos(a) * std::cos(e)));
    }

    // ---------------------------------------------------------------- UI ----
    void apply_layout() {
        dock_h = ide.vis ? std::max(150, (int)(H * 0.42f)) : 0;
        vp_x = 190; vp_y = 26; vp_w = W - 190 - 236; vp_h = H - 26 - 20 - dock_h;
        dock_y = vp_y + vp_h;
    }
    void toast(const char* msg, uint32_t col = GOLD) {
        status = msg; status_t = 3.0f; toast_col = col;
    }
    void toggle_ide() {
        ide.vis = !ide.vis;
        if (ide.vis && !ide_inited) { ide.init(); ide_inited = true; }
        apply_layout(); resize_rt(); touch();
        if (ide.vis) toast("IDE ready - Ctrl+R runs, Ctrl+S saves, F1 help");
    }

    void step_ui(Ctx& u, double dt) {
        Scene& s = rt.scene;
        if (win.key_pressed[0x70]) help_open = !help_open;           // F1
        if (help_open) { if (u.click) help_open = false; return; }   // modal
        if (win.key_pressed[0x73]) toggle_ide();                     // F2
        if (win.key_down[0x11] && win.key_pressed[0x49]) toggle_ide();  // Ctrl+I
        if (ide.vis) {
            int c;
            while ((c = win.take_char()) != -1) {
                if      (c == 0x13) ide.save();
                else if (c == 0x12) ide.do_run();
                else if (c == 0x16) ide.do_stop();
                else if (c == 27)   ide.vis = false, apply_layout(), resize_rt(), touch();
                else ide.type_char(c);
            }
            static const int NAV[] = {0x25, 0x27, 0x26, 0x28, 0x24, 0x23, 0x2E};
            for (int vk : NAV) if (win.key_pressed[vk]) ide.key(vk);
            ide.follow(std::max(2, (dock_h - 62) / 14));
            ide.run.alive();                     // reap exit status
        } else {
            while (win.take_char() != -1) {}       // drain so the ring can't grow
        }
        // camera orbit (RMB over viewport)
        if (win.rmb && u.in(vp_x, vp_y, vp_w, vp_h)) {
            cam_az -= win.mdx * 0.25f;
            cam_el = clampf(cam_el + win.mdy * 0.2f, 2.0f, 85.0f);
            touch();
        }
        if (win.wheel && u.in(vp_x, vp_y, vp_w, vp_h)) {
            cam_dist = clampf(cam_dist * (win.wheel > 0 ? 0.9f : 1.1f), 1.6f, 60.0f);
            touch();
        }
        if (autorotate) { cam_az += (float)dt * 12.0; touch(); }
        if (win.lmb && !in_panels(u.mx, u.my)) { pick(u.mx, u.my); }
        // scroll the inspector when the cursor is over it
        if (win.wheel && u.mx >= vp_x + vp_w) {
            insp_scroll = clampf(insp_scroll - win.wheel * 46.0f, 0.0f, 460.0f);
        }
        apply_cam();
    }
    bool in_panels(int x, int y) const {
        return x < vp_x || x >= vp_x + vp_w || y < vp_y || y >= vp_y + vp_h;
    }

    void draw(Ctx& u) {
        Scene& s = rt.scene;
        u.rect(0, 0, W, H, BG);

        // ---- compose viewport: RT buffer (denoised) upscaled bilinear ------
        rt.present_stream();
        if (denoise_on) rt.denoise();
        const uint8_t* src = rt.pixels();
        for (int y = 0; y < vp_h; ++y) {
            float sy = (y + 0.5f) * rt.view_h() / vp_h;
            int y0 = (int)sy; float fy = sy - y0; if (y0 >= rt.view_h() - 1) { y0 = rt.view_h() - 2; fy = 1; }
            uint8_t* d = &u.p[((size_t)(vp_y + y) * W + vp_x) * 4];
            for (int x = 0; x < vp_w; ++x) {
                float sx = (x + 0.5f) * rt.view_w() / vp_w;
                int x0 = (int)sx; float fx = sx - x0; if (x0 >= rt.view_w() - 1) { x0 = rt.view_w() - 2; fx = 1; }
                const uint8_t* a0 = src + ((size_t)y0 * rt.view_w() + x0) * 4;
                const uint8_t* b0 = a0 + 4;
                const uint8_t* a1 = a0 + (size_t)rt.view_w() * 4;
                const uint8_t* b1 = a1 + 4;
                for (int c = 0; c < 3; ++c) {
                    float v = (a0[c] * (1 - fx) + b0[c] * fx) * (1 - fy) +
                              (a1[c] * (1 - fx) + b1[c] * fx) * fy;
                    d[x * 4 + c] = (uint8_t)v;
                }
                d[x * 4 + 3] = 255;
            }
        }

        // ---- top bar ----
        u.rect(0, 0, W, 26, PANEL);
        u.text(10, 9, "NATIVE ENGINE", 2, 0xFFFFFF);
        u.text(192, 10, "RTX EDITOR", 2, ACCENT);
        tip = nullptr;
        if (button(u, "IDE", W - 236 - 392, 5, 44, 16)) toggle_ide();
        if (button(u, "HELP", W - 236 - 344, 5, 50, 16)) help_open = true;
        if (button(u, "SNAPSHOT", W - 236 - 200, 5, 86, 16)) {
            snapshot("native_engine_render.png"); toast("SNAPSHOT -> native_engine_render.png", GREEN);
        }
        if (button(u, "SAVE", W - 236 - 108, 5, 50, 16)) { save_scene("scene.nes"); toast("scene.nes saved", GREEN); }
        if (button(u, "LOAD", W - 236 - 54, 5, 50, 16))  { load_scene("scene.nes"); toast("scene.nes loaded", GREEN); }
        if (status_t > 0) { u.text(300, 10, status.c_str(), 2, toast_col); }

        // ---- left: outliner ----
        u.rect(0, 26, vp_x, vp_h, PANEL);
        u.text(10, 36, "OUTLINER", 2, TEXT);
        int row0 = 56, list_h = vp_h - 56 - 60;
        int rows = list_h / 15;
        int start = 0;
        if (sel >= rows) start = sel - rows + 1;
        for (int i = 0; i < rows && start + i < (int)s.objects.size(); ++i) {
            int oi = start + i;
            Object& o = s.objects[oi];
            bool is_sel = oi == sel;
            int y = row0 + i * 15;
            if (u.in(4, y, vp_x - 8, 15) && u.click) sel = oi;
            if (is_sel) u.rect(4, y - 1, vp_x - 8, 14, ACCENT, 0.22f);
            char b[64]; std::snprintf(b, sizeof b, "%d. %s", oi, kind_name(o));
            uint32_t mc = s.materials[o.mat >= 0 ? o.mat : 0].albedo.x > 0.9f &&
                          s.materials[o.mat >= 0 ? o.mat : 0].transmission > 0.5f ? 0x9FDFFF :
                          TEXT;
            u.text(8, y + 3, b, 1, is_sel ? 0xFFFFFF : mc);
        }
        int by = vp_y + vp_h - 52;
        if (button(u, "+SPH", 6, by, 42, 16)) add_object(ObjType::Sphere);
        if (button(u, "+BOX", 52, by, 42, 16)) add_object(ObjType::Box);
        if (button(u, "+QD", 98, by, 40, 16)) add_object(ObjType::Quad);
        if (button(u, "+PLN", 142, by, 44, 16)) add_object(ObjType::Plane);
        if (button(u, "-DEL", 6, by + 19, 42, 16)) hide_selected();
        if (button(u, "FOCUS", 52, by + 19, 46, 16) && sel >= 0) {
            cam_tgt = s.objects[sel].pos; cam_tgt.y += 0.6f; touch();
        }
        if (button(u, "AUTO", 102, by + 19, 40, 16)) autorotate = !autorotate;

        // ---- right: inspector ----
        int rx = vp_x + vp_w + 8, pw = 236 - 16;
        u.rect(vp_x + vp_w, 26, 236, vp_h, PANEL);
        u.text(rx, 36, "INSPECTOR", 2, TEXT);
        u.text(vp_x + vp_w + 200, 36, "SCROLL>", 1, DIM);
        int clip0 = vp_y + 50, clip1 = vp_y + vp_h;
        u.clipY(clip0, clip1);
        float y = 62 - insp_scroll; const float step = 30;
        auto [changed_scene, changed_post] = draw_inspector(u, rx, pw, y, step);
        u.clipY(0, 1 << 30);
        (void)changed_post;
        if (changed_scene) touch();

        // ---- bottom bar ----
        int bb = vp_y + vp_h;
        u.rect(0, bb, W, 20, PANEL);
        char b[192];
        snprintf(b, sizeof b,
                 "RT %dX%d  SCALE %.2f  PASSES %d/%d  %s  |  FPS %.0f  MRAYS/S %.1f  |  LMB PICK  RMB ORBIT  WHEEL ZOOM",
                 (int)rw, (int)rh, view_scale, (int)rt.stream_min_passes(), (int)target_passes,
                 denoise_on ? "DENOISE" : "RAW", fps, mrays);
        u.text(10, bb + 7, b, 1, DIM);
        if (status_t <= 0 && tip) {
            int tw2 = 6 * (int)std::strlen(tip);
            u.text(W - tw2 - 12, bb + 7, tip, 1, ACCENT);
        }

        // ---- docked IDE ----
        if (ide.vis && dock_h > 60) ide.draw_dock(u, 0, dock_y + 2, W, dock_h - 2, win.now());

        // ---- modal help card ----
        if (help_open) {
            u.rect(0, 0, W, H, 0x000000, 0.72f);
            int hx = (W - 640) / 2, hy = (H - 360) / 2;
            u.rect(hx, hy, 640, 360, PANEL);
            u.frame_rect(hx, hy, 640, 360, ACCENT);
            u.text(hx + 20, hy + 14, "NATIVE ENGINE - CONTROLS", 2, 0xFFFFFF);
            const char* L[] = {
                "VIEWPORT   LMB pick  |  RMB drag orbit  |  wheel zoom",
                "OUTLINER   +SPH +BOX +QD +PLN add  |  -DEL hides  |  FOCUS centers",
                "             AUTO toggles camera auto-orbit",
                "INSPECTOR  wheel over the panel scrolls it; sliders drag live,",
                "             the RT stream re-queues each frame you edit",
                "RENDER     bounces, exposure, RT SCALE (0.25-1.0), TARGET SPP,",
                "             FRAME BUDGET (ms per UI frame), DENOISE, REFLOW",
                "IDE   F2 / Ctrl+I / IDE button - docked code editor:",
                "             NEW PY   starter game using the native_engine API",
                "             NEW CPP  standalone C++ sample, built with your g++",
                "             RUN (Ctrl+R) streams output into the console",
                "             STOP kills the process tree; files live in projects/",
                "SNAPSHOT saves a converged full-res render to native_engine_render.png",
                "SAVE / LOAD write scene.nes  |  F1 or click closes this card",
            };
            for (int i = 0; i < 14; ++i)
                u.text(hx + 20, hy + 46 + i * 21, L[i], 1,
                       i >= 7 ? ACCENT : TEXT);
        }
    }

    std::pair<bool, bool> draw_inspector(Ctx& u, int rx, int pw, float& y, float step);
};

// Inspector implemented out-of-line for readability.
std::pair<bool, bool> Editor::draw_inspector(Ctx& u, int rx, int pw, float& y, float step) {
    Scene& s = rt.scene;
    bool scene_dirty = false, post_dirty = false;

    if (sel >= 0 && sel < (int)s.objects.size()) {
        Object& o = s.objects[sel];
        u.text(rx, (int)y, "OBJECT", 2, GOLD); y += 14;
        Vec3 p0 = o.pos; float r0 = o.radius; Vec3 h0 = o.half; float rot0 = o.rot_y;
        float du0 = o.d, qu0 = o.q_u.y, qv0 = length(o.q_v);
        auto fd = [](float a, float b){ return std::fabs(a-b) > 1e-5f; };
        o.pos.x = slider(u, "POS X", rx, y, pw, o.pos.x, -30, 30, "%.2f"); y += step;
        o.pos.y = slider(u, "POS Y", rx, y, pw, o.pos.y, -2, 30, "%.2f"); y += step;
        o.pos.z = slider(u, "POS Z", rx, y, pw, o.pos.z, -30, 30, "%.2f"); y += step;
        if (o.type == ObjType::Sphere) {
            o.radius = slider(u, "RADIUS", rx, y, pw, o.radius, 0.05f, 5.0f, "%.2f", PINK); y += step;
        }
        if (o.type == ObjType::Box) {
            o.half.x = slider(u, "HALF X", rx, y, pw, o.half.x, 0.05f, 6, "%.2f", PINK); y += step;
            o.half.y = slider(u, "HALF Y", rx, y, pw, o.half.y, 0.05f, 6, "%.2f", PINK); y += step;
            o.half.z = slider(u, "HALF Z", rx, y, pw, o.half.z, 0.05f, 6, "%.2f", PINK); y += step;
            o.rot_y = slider(u, "ROT Y", rx, y, pw, o.rot_y, -180, 180, "%.0f", PINK); y += step;
        }
        if (o.type == ObjType::Quad) {
            o.q_u.y = slider(u, "U HEIGHT", rx, y, pw, o.q_u.y, 0.2f, 8, "%.2f", PINK); y += step;
            float len = length(o.q_v);
            len = slider(u, "V SIZE", rx, y, pw, len, 0.2f, 12, "%.2f", PINK); y += step;
            o.q_v = normalize(o.q_v) * len;
            o.q_normal = normalize(cross(o.q_u, o.q_v)); o.q_area = length(cross(o.q_u, o.q_v));
        }
        if (o.type == ObjType::Plane) {
            o.d = slider(u, "HEIGHT Y", rx, y, pw, o.d, -5, 20, "%.2f", PINK); y += step;
        }
        scene_dirty = fd(p0.x,o.pos.x)||fd(p0.y,o.pos.y)||fd(p0.z,o.pos.z)||fd(r0,o.radius)||
                      fd(rot0,o.rot_y)||fd(h0.x,o.half.x)||fd(h0.y,o.half.y)||fd(h0.z,o.half.z)||
                      fd(du0,o.d)||fd(qu0,o.q_u.y)||fd(qv0,length(o.q_v));
        y += 6;
        if (o.mat >= 0 && o.mat < (int)s.materials.size()) {
            Material& m = s.materials[o.mat];
            u.text(rx, (int)y, "MATERIAL", 2, GREEN); y += 14;
            Vec3 a0 = m.albedo, e0 = m.emissive;
            float mt0=m.metallic, rg0=m.roughness, io0=m.ior, tr0=m.transmission, ck0=m.checker;
            m.albedo.x = slider(u, "ALBEDO R", rx, y, pw, m.albedo.x, 0, 1, "%.2f"); y += step;
            m.albedo.y = slider(u, "ALBEDO G", rx, y, pw, m.albedo.y, 0, 1, "%.2f"); y += step;
            m.albedo.z = slider(u, "ALBEDO B", rx, y, pw, m.albedo.z, 0, 1, "%.2f"); y += step;
            m.metallic = slider(u, "METALLIC", rx, y, pw, m.metallic, 0, 1, "%.2f", GOLD); y += step;
            m.roughness = slider(u, "ROUGHNESS", rx, y, pw, m.roughness, 0.02f, 1, "%.2f"); y += step;
            m.ior = slider(u, "IOR", rx, y, pw, m.ior, 1.0f, 2.4f, "%.2f"); y += step;
            m.transmission = slider(u, "GLASS", rx, y, pw, m.transmission, 0, 1, "%.2f", ACCENT); y += step;
            float em0 = m.emissive.x + m.emissive.y + m.emissive.z;
            float nem = slider(u, "EMISSION", rx, y, pw, em0, 0, 30, "%.1f", PINK);
            if (nem != em0) {
                Vec3 t = m.emissive;
                float nsum = t.x + t.y + t.z;
                Vec3 dir = nsum > 1e-5f ? t * (1.0f / nsum) : Vec3(1, 0.62f, 0.3f);
                m.emissive = dir * std::max(0.0f, nem);
            }
            y += step;
            m.checker = slider(u, "CHECKER", rx, y, pw, m.checker, 0, 4, "%.2f"); y += step;
            scene_dirty = scene_dirty || fd(mt0,m.metallic)||fd(rg0,m.roughness)||fd(io0,m.ior)||
                          fd(tr0,m.transmission)||fd(ck0,m.checker)||
                          fd(a0.x,m.albedo.x)||fd(a0.y,m.albedo.y)||fd(a0.z,m.albedo.z)||
                          fd(e0.x,m.emissive.x)||fd(e0.y,m.emissive.y)||fd(e0.z,m.emissive.z);
        }
        y += 8;
    }

    u.text(rx, (int)y, "SKY & LIGHT", 2, TEXT); y += 14;
    {
        Sky& k = s.sky;
        float az = sun_az(), el = sun_el();
        float naz = slider(u, "SUN AZ", rx, y, pw, az, -180, 180, "%.0f", GOLD); y += step;
        float nel = slider(u, "SUN EL", rx, y, pw, el, 3, 85, "%.0f", GOLD); y += step;
        if (naz != az || nel != el) { set_sun(naz, nel); scene_dirty = true; }
        float ni = slider(u, "SUN INT", rx, y, pw, k.sun_intensity, 0, 12, "%.1f", GOLD); y += step;
        if (ni != k.sun_intensity) { k.sun_intensity = ni; scene_dirty = true; }
        float nr = slider(u, "SUN SIZE DEG", rx, y, pw,
                          std::acos(clampf(k.sun_cos_angle, -1, 1)) * 180.0f / NE_PI * 2.0f,
                          0.4f, 8.0f, "%.1f", GOLD); y += step;
        float nrad = std::max(0.15f, nr * 0.5f);
        float nca = std::cos(nrad * NE_PI / 180.0f);
        if (nca != k.sun_cos_angle) { k.sun_cos_angle = nca; scene_dirty = true; }
        float sti = slider(u, "SKY INT", rx, y, pw, k.intensity, 0, 3, "%.2f"); y += step;
        if (sti != k.intensity) { k.intensity = sti; scene_dirty = true; }
        k.top.x = slider(u, "SKY TOP R", rx, y, pw, k.top.x, 0, 1, "%.2f"); y += step;
        k.top.y = slider(u, "SKY TOP G", rx, y, pw, k.top.y, 0, 1, "%.2f"); y += step;
        k.top.z = slider(u, "SKY TOP B", rx, y, pw, k.top.z, 0, 1, "%.2f"); y += step;
        k.horizon.x = slider(u, "HORIZON R", rx, y, pw, k.horizon.x, 0, 1, "%.2f"); y += step;
        k.horizon.y = slider(u, "HORIZON G", rx, y, pw, k.horizon.y, 0, 1, "%.2f"); y += step;
        k.horizon.z = slider(u, "HORIZON B", rx, y, pw, k.horizon.z, 0, 1, "%.2f"); y += step;
    }
    y += 8;
    u.text(rx, (int)y, "RENDER", 2, ACCENT); y += 14;
    {
        float b = (float)s.max_bounces;
        float nb = slider(u, "M BOUNCES", rx, y, pw, b, 1, 12, "%.0f"); y += step;
        if ((int)nb != s.max_bounces) { s.max_bounces = (int)nb; scene_dirty = true; }
        float ex = slider(u, "EXPOSURE", rx, y, pw, rt.exposure, 0.2f, 2.5f, "%.2f"); y += step;
        if (ex != rt.exposure) { rt.exposure = ex; post_dirty = true; }
        float sc = slider(u, "RT SCALE", rx, y, pw, view_scale, 0.25f, 1.0f, "%.2f"); y += step;
        if (std::fabs(sc - view_scale) > 0.02f) { view_scale = sc; resize_rt(); scene_dirty = true; }
        float tg = (float)target_passes;
        float ntg = slider(u, "TARGET SPP", rx, y, pw, tg, 1, 64, "%.0f"); y += step;
        if ((uint32_t)ntg != target_passes) { target_passes = (uint32_t)std::max(1.0f, ntg); scene_dirty = true; }
        float bd = (float)budget_ms;
        float nbd = slider(u, "FRAME BUDGET MS", rx, y, pw, bd, 1, 20, "%.0f"); y += step;
        if ((int)nbd != budget_ms) { budget_ms = (int)nbd; }
        if (toggle(u, "DENOISE", rx, y, denoise_on)) post_dirty = true; y += 20;
        if (toggle(u, "AUTO-ORBIT", rx, y, autorotate)) {} y += 20;
        if (button(u, "REFLOW", rx, y, 70, 16)) scene_dirty = true;
        y += 22;
    }
    return {scene_dirty, post_dirty};
}

} // namespace ed

// ---------------------------------------------------------------------------
//  Application driver
// ---------------------------------------------------------------------------
struct App : ed::Editor {
    void loop(int headless_frames, const char* headless_shot) {
        double prev = win.now();
        while (win.pump()) {
            double t = win.now();
            double dt = t - prev; prev = t;
            if (dt > 0.25) dt = 0.25;
            fps += (1.0 / (dt + 1e-5) - fps) * 0.1;

            ed::Ctx u{disp.data(), W, H, (int)win.mx, (int)win.my, win.lmb, (bool)win.lmb_edge};
            step_ui(u, dt);

            // ---- RT streaming: spend the frame budget, never block UI ----
            uint64_t rays0 = rt.scene.ray_count.load();
            int guard = 0;
            while (rt.stream_step(budget_ms * 1000) && ++guard < 4) {}
            double rays = (double)(rt.scene.ray_count.load() - rays0);
            if (dt > 1e-4) mrays = mrays * 0.88 + (rays / 1e6 / dt) * 0.12;
            if (status_t > 0) status_t -= (float)dt;

            draw(u);
            win.present(disp.data());
            ++frame_no;
            if (headless_frames > 0 && frame_no == 30) {
                // scripted: click the glass sphere + scroll the inspector
                win.mx = (float)(vp_x + vp_w / 2);
                win.my = (float)(vp_y + vp_h / 2);
                win.lmb = true; win.lmb_edge = 1;
            } else if (headless_frames > 0 && frame_no == 31) {
                win.lmb = false;
            }
            if (headless_frames > 0 && frame_no >= 120 && frame_no <= 140) {
                insp_scroll = clampf(insp_scroll + 34.0f, 0.0f, 460.0f);
            }
            // scripted IDE demo: open, type a script, run it, watch console
            if (headless_frames > 0 && frame_no == 60) toggle_ide();
            static const char* IDE_TYPE = "print('hello from the built-in IDE')\n";
            if (headless_frames > 0 && frame_no > 75 && frame_no < 140) {
                if (!ide_typed) { ide.new_file(".py"); ide_typed = true; }
                int i = frame_no - 76;
                if (i >= 0 && i < (int)std::strlen(IDE_TYPE)) ide.type_char(IDE_TYPE[i]);
            }
            if (headless_frames > 0 && frame_no == 150) ide.do_run();
            if (!rt.stream_active() && !autorotate) {
#ifdef _WIN32
                Sleep(4);
#else
                struct timespec ts{0, 4 * 1000 * 1000L};
                nanosleep(&ts, nullptr);
#endif
            }
            if (headless_frames > 0 && frame_no >= headless_frames) {
                if (headless_shot && *headless_shot) {
                    auto png = encode_png(disp.data(), W, H);
                    FILE* f = fopen(headless_shot, "wb");
                    if (f) { fwrite(png.data(), 1, png.size(), f); fclose(f); }
                }
                break;
            }
        }
    }
    bool open(int w, int h) {
        W = w; H = h;
        if (!win.create("Native Engine - RTX Editor & IDE", w, h)) return false;
        disp.assign((size_t)w * h * 4, 0);
        apply_layout();
        rt.resize(256, 256);
        default_scene();
        apply_cam();
        resize_rt();
        touch();
        return true;
    }
};

extern "C" int ne_editor_main(int w, int h, int frame_limit, const char* shot) {
    App app;
    if (!app.open(w > 0 ? w : 1280, h > 0 ? h : 720)) return 1;
    printf("Native Engine RTX Editor | %dx%d | rt viewport %dx%d @%.2f | threads: %u\n",
           app.W, app.H, app.rw, app.rh, app.view_scale, ne::hardware_threads());
    app.loop(frame_limit, shot);
    return 0;
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return ne_editor_main(1280, 720, 0, nullptr);
}
#else
int main(int argc, char** argv) {
    int w = 1280, h = 720, frames = 0; const char* shot = nullptr;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!strcmp(argv[i], "--w")) w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--h")) h = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--headless")) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot")) shot = argv[++i];
    }
    return ne_editor_main(w, h, frames, shot);
}
#endif

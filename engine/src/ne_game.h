// ============================================================================
//  Native Engine — ne_game.h
//  "NEON RUNNER" — a playable demo proving the engine is a GAME engine:
//  entity system, physics (gravity/jump/bounce/collision), camera follow,
//  particles, HUD, lives/score/timer, RT-cinematic-mode hotkey.
//  The same Scene feeds both the 60 fps rasterizer and the path tracer.
// ============================================================================
#pragma once

#include "ne_scene.h"
#include "ne_renderer.h"  // full Renderer type for RT mode
#include "ne_raster.h"
#include "ne_window.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ne {

class Game {
public:
    Scene& scene;
    Raster raster;
    Window* win = nullptr;

    // ---- entity kinds -------------------------------------------------------
    enum Kind { Player = 0, Crystal, Enemy, Particle, Lamp };
    struct Ent {
        Vec3 p, v;
        float r = 0.3f, life = 0.0f;
        int obj = -1;        // index into scene.objects
        float phase = 0.0f;
    };
    std::vector<Ent> ents;

    int score = 0, lives = 3;
    float elapsed = 0.0f, fps = 60.0f;
    bool over = false, won = false, rt_mode = false;
    int rt_passes = 0;
    float invuln = 0.0f, damage_flash = 0.0f;

    float cam_yaw = -0.6f, cam_pitch = 0.42f, cam_dist = 7.5f;
    Vec3 cam_target;

    int m_floor, m_player, m_crystal, m_enemy, m_wall, m_lamp;
    int m_p[3];               // particle tints
    int obj_ground = -1;
    float arena = 16.0f;
    int n_crystals = 12;
    unsigned rng_state = 0xB0BA57u;
    int pstart = 16;          // index where the particle pool starts in ents
    Renderer* rt = nullptr;   // optional: enables F1 cinematic RT mode

    explicit Game(Scene& s) : scene(s) {}

    unsigned rnd() { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }
    float rndf() { return (float)(rnd() & 0xFFFFFF) / (float)0x1000000; }
    float rndr(float a, float b) { return a + (b - a) * rndf(); }

    // ------------------------------------------------------------------ build
    int add_mat(float al[3], float met, float rough, float em[3], float checker = 0) {
        Material m;
        m.albedo = Vec3(al[0], al[1], al[2]);
        m.metallic = met; m.roughness = rough; m.checker = checker;
        if (em) m.emissive = Vec3(em[0], em[1], em[2]);
        return scene.add_material(m);
    }
    int add_sphere_mat(int mat, Vec3 p, float r) {
        Object o; o.type = ObjType::Sphere; o.mat = mat; o.pos = p; o.radius = r;
        scene.add_object(o);
        return (int)scene.objects.size() - 1;
    }
    int add_box_mat(int mat, Vec3 p, Vec3 half) {
        Object o; o.type = ObjType::Box; o.mat = mat; o.pos = p; o.half = half;
        scene.add_object(o);
        return (int)scene.objects.size() - 1;
    }

    void build() {
        scene.materials.clear(); scene.objects.clear(); scene.lights.clear();
        ents.clear();
        float whi[3] = {0.78f, 0.78f, 0.82f}, dark[3] = {0.05f, 0.06f, 0.10f};
        float grn[3] = {0.15f, 1.4f, 0.5f}, red[3] = {0.85f, 0.06f, 0.06f};
        float org[3] = {1.2f, 0.55f, 0.18f}, gold[3] = {1.0f, 0.71f, 0.29f};
        m_floor = add_mat(whi, 0.0f, 0.42f, nullptr, 1.7f);
        m_player = add_mat(gold, 1.0f, 0.22f, nullptr);
        m_crystal = add_mat(grn, 0.0f, 0.25f, grn);
        m_enemy = add_mat(red, 0.7f, 0.35f, red);
        m_wall = add_mat(dark, 0.1f, 0.5f, nullptr);
        m_lamp = add_mat(org, 0.0f, 0.3f, org);
        float pg[3] = {0.5f, 1.5f, 0.7f}, pr[3] = {1.5f, 0.4f, 0.3f}, pb[3] = {0.4f, 0.8f, 1.6f};
        m_p[0] = add_mat(pg, 0, 1, pg); m_p[1] = add_mat(pr, 0, 1, pr); m_p[2] = add_mat(pb, 0, 1, pb);

        { Object o; o.type = ObjType::Plane; o.mat = m_floor; o.normal = Vec3(0, 1, 0); o.d = 0;
          scene.add_object(o); obj_ground = 0; }

        // arena walls (also visual horizon blockers)
        for (int s = 0; s < 4; ++s) {
            bool xw = s < 2; float sgn = (s & 1) ? 1 : -1;
            Vec3 p = xw ? Vec3(sgn * arena, 1.5f, 0) : Vec3(0, 1.5f, sgn * arena);
            Vec3 h = xw ? Vec3(0.5f, 1.5f, arena + 1) : Vec3(arena + 1, 1.5f, 0.5f);
            add_box_mat(m_wall, p, h);
        }
        // corner lamps (area lights for RT mode)
        for (int i = 0; i < 4; ++i) {
            float sx = (i & 1) ? 1 : -1, sz = (i & 2) ? 1 : -1;
            add_box_mat(m_wall, Vec3(sx * (arena - 1.2f), 1.9f, sz * (arena - 1.2f)), Vec3(0.16f, 1.9f, 0.16f));
            add_sphere_mat(m_lamp, Vec3(sx * (arena - 1.2f), 3.95f, sz * (arena - 1.2f)), 0.34f);
        }
        // player
        ents.push_back({Vec3(0, 0.85f, 6), Vec3(), 0.62f, 0, add_sphere_mat(m_player, Vec3(0, 0.85f, 6), 0.62f), 0});
        // crystals
        for (int i = 0; i < n_crystals; ++i) {
            Vec3 p(rndr(-arena + 2, arena - 2), 0.55f, rndr(-arena + 2, arena - 2));
            ents.push_back({p, Vec3(), 0.34f, 1.0f, add_sphere_mat(m_crystal, p, 0.34f), rndf() * 6.28f});
        }
        // enemies
        for (int i = 0; i < 3; ++i) {
            Vec3 p(rndr(-arena + 3, arena - 3), 0.9f, rndr(-arena + 3, arena - 3));
            ents.push_back({p, Vec3(), 0.5f, 1.0f, add_sphere_mat(m_enemy, p, 0.5f), rndf() * 6.28f});
        }
        // particle pool
        pstart = (int)ents.size();
        for (int i = 0; i < 160; ++i) {
            int mat = m_p[i % 3];
            ents.push_back({Vec3(0, 1e6f, 0), Vec3(), 0.11f, 0, add_sphere_mat(mat, Vec3(0, 1e6f, 0), 0.11f), 0});
        }

        // dusk neon sky + low warm sun
        scene.sky.top = Vec3(0.05f, 0.07f, 0.16f);
        scene.sky.horizon = Vec3(0.85f, 0.35f, 0.42f);
        scene.sky.intensity = 0.85f;
        scene.sky.sun_dir = normalize(Vec3(0.18f, 0.22f, -1.0f));
        scene.sky.sun_color = Vec3(1.0f, 0.62f, 0.42f);
        scene.sky.sun_intensity = 2.6f;
        scene.sky.sun_cos_angle = std::cos(3.5f * NE_PI / 180.0f);
        scene.max_bounces = 4;

        score = 0; lives = 3; elapsed = 0; over = false; won = false;
        invuln = 0; damage_flash = 0; rt_mode = false; rt_passes = 0;
    }

    Ent& player() { return ents[0]; }

    // ------------------------------------------------------------------ step
    void sync() {   // push entity state into the shared Scene
        for (auto& e : ents) {
            scene.objects[e.obj].pos = e.p;
            scene.objects[e.obj].radius = e.r;
        }
        scene.camera.pos = eye_pos();
        scene.camera.target = cam_target;
        scene.camera.fov_y_deg = 62.0f;
    }

    Vec3 eye_pos() const {
        float cp = std::cos(cam_pitch), sp = std::sin(cam_pitch);
        return cam_target + Vec3(std::sin(cam_yaw) * cp, sp, std::cos(cam_yaw) * cp) * cam_dist;
    }

    void burst(Vec3 p, int n, float speed) {
        int born = 0;
        for (auto& e : ents)
            if (e.life <= 0.0f && (&e - &ents[0]) >= pstart) {
                e.p = p;
                float a = rndf() * 6.28f, u = rndf() * 1.4f;
                e.v = Vec3(std::cos(a) * (0.5f + u), 2.2f + u * 2.0f, std::sin(a) * (0.5f + u)) * speed;
                e.life = 0.55f + rndf() * 0.5f;
                if (++born >= n) break;
            }
    }

    void update(float dt) {
        elapsed += (over || won) ? 0.0f : dt;
        Window& w = *win;
        auto held = [&](int vk) { return w.key_down[vk] != 0; };

        // restart
#ifdef _WIN32
        if (w.key_pressed[0x52]) { build(); return; } // R
        if (w.key_pressed[0x70]) { rt_mode = !rt_mode; rt_passes = 0; if (rt) rt->reset_accum(); } // F1
#else
        if (w.key_pressed[0x52]) { build(); return; }
        if (w.key_pressed[0x70]) { rt_mode = !rt_mode; rt_passes = 0; if (rt) rt->reset_accum(); }
#endif

        Ent& pl = player();
        if (!over && !won) {
            // camera control: right-drag to orbit, Q/E zoom, F/G pitch preset
            if (w.rmb) { cam_yaw -= w.mdx * 0.0055f; cam_pitch = clampf(cam_pitch + w.mdy * 0.004f, 0.12f, 1.15f); }
            if (held('Q')) cam_dist = clampf(cam_dist - 6.0f * dt, 3.2f, 14.0f);
            if (held('E')) cam_dist = clampf(cam_dist + 6.0f * dt, 3.2f, 14.0f);

            // movement in camera space
            Vec3 fwd = normalize(cam_target - eye_pos()); fwd.y = 0; fwd = normalize(fwd);
            Vec3 rgt(-fwd.z, 0, fwd.x);
            Vec3 acc(0);
            if (held('W')) acc += fwd;
            if (held('S')) acc -= fwd;
            if (held('A')) acc -= rgt;
            if (held('D')) acc += rgt;
            float boost = held(0x10) ? 2.1f : 1.0f; // SHIFT
            if (length_sq(acc) > 0) acc = normalize(acc) * (46.0f * boost);
            pl.v += acc * dt;
            pl.v *= std::pow(0.0018f, dt);          // strong ground friction
            pl.v.y -= 34.0f * dt;
            pl.p += pl.v * dt;
            float floor_y = pl.r;
            if (pl.p.y < floor_y) { pl.p.y = floor_y; if (pl.v.y < 0) pl.v.y = pl.v.y < -6 ? -pl.v.y * 0.35f : 0; }
            static bool was_grounded = true;
            bool grounded = pl.p.y <= floor_y + 0.001f;
            if (grounded && w.key_down[0x20]) pl.v.y = 11.5f; // SPACE jump
            was_grounded = grounded;
            // walls
            float lim = arena - 0.8f - pl.r;
            if (pl.p.x < -lim) { pl.p.x = -lim; pl.v.x = std::fabs(pl.v.x) * 0.3f; }
            if (pl.p.x >  lim) { pl.p.x =  lim; pl.v.x = -std::fabs(pl.v.x) * 0.3f; }
            if (pl.p.z < -lim) { pl.p.z = -lim; pl.v.z = std::fabs(pl.v.z) * 0.3f; }
            if (pl.p.z >  lim) { pl.p.z =  lim; pl.v.z = -std::fabs(pl.v.z) * 0.3f; }
        }

        // follow camera (spring)
        Vec3 want = pl.p + Vec3(0, pl.r + 0.55f, 0);
        float k = 1.0f - std::exp(-7.0f * dt);
        cam_target += (want - cam_target) * k;

        size_t base = 1;                       // crystals start after the player
        for (int i = 0; i < n_crystals; ++i) { // crystals
            Ent& c = ents[base + i];
            c.p.y = 0.55f + 0.22f * std::sin(elapsed * 2.4f + c.phase);
            c.phase += 0.0f;
            if (!over && !won && length(c.p - pl.p) < pl.r + c.r + 0.55f) {
                score++;
                burst(c.p, 18, 1.0f);
#ifdef _WIN32
                MessageBeep(0xFFFFFFFF);
#endif
                if (score >= n_crystals) { won = true; burst(pl.p, 40, 1.6f); }
                else {
                    c.p = Vec3(rndr(-arena + 2, arena - 2), 0.55f, rndr(-arena + 2, arena - 2));
                    if (length(c.p - pl.p) < 5.0f) {   // teleport away, no chaining
                    c.p = Vec3(rndr(-arena + 2, arena - 2), 0.55f, rndr(-arena + 2, arena - 2));
                }
                }
            }
        }
        size_t eb = base + n_crystals;
        for (int i = 0; i < 3; ++i) {          // enemies chase
            Ent& en = ents[eb + i];
            if (over || won) continue;
            Vec3 to = pl.p - en.p; to.y = 0;
            float d = length(to);
            float spd = 3.1f + 0.32f * score + (float)i * 0.35f;
            if (d > 0.001f) en.v = en.v * 0.85f + normalize(to) * (spd * 0.15f);
            en.p += en.v * dt;
            en.p.y = 0.9f + 0.42f * std::sin(elapsed * 3.0f + en.phase);
            float lim = arena - 0.9f;
            en.p.x = clampf(en.p.x, -lim, lim); en.p.z = clampf(en.p.z, -lim, lim);
            if (invuln <= 0 && d < pl.r + en.r + 0.15f) {
                lives--; invuln = 1.6f; damage_flash = 0.45f;
                pl.v = normalize(Vec3(pl.p.x - en.p.x, 0, pl.p.z - en.p.z)) * 9.0f + Vec3(0, 6.5f, 0);
                burst(pl.p, 14, 0.8f);
                if (lives <= 0) over = true;
            }
        }
        if (invuln > 0) invuln -= dt;
        if (damage_flash > 0) damage_flash -= dt;

        size_t pb = eb + 3;                      // particles
        for (size_t i = pb; i < ents.size(); ++i) {
            Ent& e = ents[i];
            if (e.life <= 0) continue;
            e.life -= dt;
            e.v.y -= 16.0f * dt;
            e.p += e.v * dt;
            if (e.p.y < 0.09f) { e.p.y = 0.09f; e.v.y = std::fabs(e.v.y) * 0.42f; }
            e.r = clampf(e.life * 0.28f, 0.0f, 0.13f);
            if (e.life <= 0) { e.p.y = 1e6f; e.r = 0.0002f; }
        }
        sync();
    }

    // -------------------------------------------------------------- drawing --
    void draw_hud() {
        char buf[192];
        uint32_t white = 0xFFFFFF, cyan = 0x35E7FF, pink = 0xFF4FB2, gold = 0xFFC93D;
        Raster& R = raster;
        std::snprintf(buf, sizeof buf, "NEON RUNNER   CRYSTALS %d/%d", score, n_crystals);
        R.text(14, 12, buf, 3, white);
        std::snprintf(buf, sizeof buf, "TIME %2.1f S   FPS %2.0f", elapsed, fps);
        R.text(14, 30, buf, 2, 0x9AA6C0);
        std::snprintf(buf, sizeof buf, "LIVES ");
        R.text(14, 48, buf, 3, pink);
        for (int i = 0; i < lives; ++i) R.text(66 + i * 14, 48, "*", 3, pink);

        std::snprintf(buf, sizeof buf, "WASD MOVE  SPACE JUMP  SHIFT BOOST  RMB ORBIT  Q E ZOOM  F1 RT-MODE  R RESTART");
        R.text(14, R.height - 18, buf, 2, 0x7E8AA6);

        if (rt_mode && rt) {
            std::snprintf(buf, sizeof buf, "RAY-TRACED MODE - PASS %d  (ACCUMULATING...)", rt_passes);
            R.text((R.width - 62 * 12) / 2, R.height - 40, buf, 2, cyan);
        }
        if (damage_flash > 0) {
            uint8_t a = (uint8_t)(damage_flash * 190.0f);
            for (int y = 0; y < R.height; ++y)
                for (int x = 0; x < R.width; ++x) {
                    size_t j = ((size_t)y * R.width + x) * 4;
                    if (R.rgba[j + 1] > a / 2) R.rgba[j + 1] = 0; // crush green on damage
                    if (R.rgba[j + 2] > a / 2) R.rgba[j + 2] = 0;
                    R.rgba[j] = (uint8_t)std::min(255, R.rgba[j] + a);
                }
        }
        if (won || over) {
            const char* msg = won ? "YOU GOT THEM ALL - NEON CHAMPION!" : "CAPTURED - PRESS R TO RUN AGAIN";
            int w = (int)std::strlen(msg);
            R.text((R.width - w * 16) / 2, R.height / 2 - 20, msg, 4, won ? gold : pink);
            char s2[96];
            std::snprintf(s2, sizeof s2, "SCORE %d   TIME %2.1F", score, elapsed);
            R.text((R.width - (int)std::strlen(s2) * 8) / 2, R.height / 2 + 14, s2, 2, white);
        }
    }

    void render_frame() {
        Vec3 eye = eye_pos();
        if (rt_mode && rt) {
            rt_passes++;
            rt->render(2);                 // progressive path-traced frame
            rt->get_rgba(raster.rgba);     // present RT output directly
            draw_hud();
        } else {
            raster.render(scene, eye, cam_target, 62.0f);
            draw_hud();
        }
        if (win) win->present(raster.rgba.data());
    }
};

} // namespace ne

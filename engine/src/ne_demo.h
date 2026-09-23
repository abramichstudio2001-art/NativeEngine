// ============================================================================
//  Native Engine — ne_demo.h
//  The signature "RTX Showcase" scene, built through the public C ABI.
//  (The Python demo rebuilds this same scene through the Python bindings.)
// ============================================================================
#pragma once

#include "ne_api.h"
#include <cmath>
#include <vector>
#include <cstring>

namespace ne {

inline void add_torus_mesh(NEEngine* e, int mat, float R, float r, int seg_u, int seg_v,
                           float tilt_x_deg, const float pos[3], float rot_y_deg) {
    std::vector<float> verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)(seg_u + 1) * (seg_v + 1) * 3);
    float tilt = tilt_x_deg * 3.14159265f / 180.0f;
    float ct = std::cos(tilt), st = std::sin(tilt);
    for (int i = 0; i <= seg_u; ++i) {
        float u = i * 2.0f * 3.14159265f / seg_u;
        float cu = std::cos(u), su = std::sin(u);
        for (int j = 0; j <= seg_v; ++j) {
            float v = j * 2.0f * 3.14159265f / seg_v;
            float cv = std::cos(v), sv = std::sin(v);
            float x = (R + r * cv) * cu;
            float y = r * sv;
            float z = (R + r * cv) * su;
            // bake tilt around X
            float ty = y * ct - z * st;
            float tz = y * st + z * ct;
            verts.push_back(x); verts.push_back(ty); verts.push_back(tz);
        }
    }
    int W = seg_v + 1;
    for (int i = 0; i < seg_u; ++i)
        for (int j = 0; j < seg_v; ++j) {
            unsigned a = i * W + j, b = (i + 1) * W + j;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    ne_add_mesh(e, mat, verts.data(), (int)verts.size() / 3, idx.data(), (int)idx.size());
    ne_set_object_transform(e, ne_stat_objects(e) - 1, pos, rot_y_deg);
}

// Orbiting hero camera at time angle `theta_deg`.
inline void set_showcase_camera(NEEngine* e, float theta_deg) {
    float t = theta_deg * 3.14159265f / 180.0f;
    float px = 6.4f * std::sin(t);
    float pz = 6.4f * std::cos(t);
    float py = 2.35f - 0.35f * std::cos(t * 0.5f);
    float pos[3] = {px, py, pz};
    float tgt[3] = {0.0f, 0.95f, -0.2f};
    ne_set_camera(e, pos, tgt, 50.0f, 0.055f, 6.6f);
}

// Build the showcase scene into `e`.
inline void build_showcase_scene(NEEngine* e) {
    // ---------------- materials ----------------
    float c_white[3] = {0.85f, 0.83f, 0.80f};
    float zero[3] = {0, 0, 0};
    int mat_ground = 0;
    {
        float a[3] = {0.85f, 0.83f, 0.80f};
        mat_ground = ne_add_material(e, a, 0.0f, 0.32f, zero, 1.5f, 0.0f, 1.25f); // checker floor
    }
    int mat_gold = ne_add_material(e, c_white, 0, 0, zero, 1.5f, 0, 0); // placeholder, fixed below
    {
        float a[3] = {1.00f, 0.71f, 0.29f};
        ne_update_material(e, mat_gold, a, 1.0f, 0.14f, zero);
    }
    int mat_glass;
    {
        float a[3] = {0.97f, 0.99f, 1.00f};
        mat_glass = ne_add_material(e, a, 0.0f, 0.03f, zero, 1.52f, 1.0f, 0.0f);
    }
    int mat_red;
    {
        float a[3] = {0.78f, 0.12f, 0.10f};
        mat_red = ne_add_material(e, a, 0.0f, 0.85f, zero, 1.5f, 0.0f, 0.0f);
    }
    int mat_ring;
    {
        float a[3] = {0.95f, 0.96f, 0.98f};
        mat_ring = ne_add_material(e, a, 1.0f, 0.22f, zero, 1.5f, 0.0f, 0.0f);
    }
    int mat_magenta;
    {
        float a[3] = {0.2f, 0.02f, 0.25f};
        float em[3] = {8.5f, 1.4f, 8.0f};
        mat_magenta = ne_add_material(e, a, 0.0f, 1.0f, em, 1.5f, 0.0f, 0.0f);
    }
    int mat_cyan;
    {
        float a[3] = {0.02f, 0.18f, 0.22f};
        float em[3] = {1.1f, 7.5f, 9.5f};
        mat_cyan = ne_add_material(e, a, 0.0f, 1.0f, em, 1.5f, 0.0f, 0.0f);
    }
    int mat_bulb;
    {
        float em[3] = {14.0f, 8.5f, 4.2f};
        mat_bulb = ne_add_material(e, c_white, 0.0f, 1.0f, em, 1.5f, 0.0f, 0.0f);
    }

    // ---------------- geometry ----------------
    ne_add_plane(e, mat_ground, 0.0f);

    float gp[3] = {0.0f, 1.05f, -0.2f};
    ne_add_sphere(e, mat_glass, gp, 1.0f); // hero glass sphere

    // metal ring around the glass sphere (tilted torus = "Saturn" ring)
    float rp[3] = {0.0f, 1.05f, -0.2f};
    add_torus_mesh(e, mat_ring, 1.58f, 0.235f, 96, 40, -28.0f, rp, 0.0f);

    float au[3] = {-2.6f, 0.78f, 1.2f};
    ne_add_sphere(e, mat_gold, au, 0.78f);

    float ru[3] = {2.5f, 0.68f, 1.0f};
    ne_add_sphere(e, mat_red, ru, 0.68f);

    // emissive studio panels
    {
        float p0[3] = {-5.6f, 0.4f, -2.0f}, u[3] = {0.0f, 2.6f, 0.0f}, v[3] = {0.0f, 0.0f, 4.0f};
        ne_add_quad(e, mat_magenta, p0, u, v);
    }
    {
        float p0[3] = {3.2f, 0.4f, -5.6f}, u[3] = {3.2f, 0.0f, 0.0f}, v[3] = {0.0f, 2.6f, 0.0f};
        ne_add_quad(e, mat_cyan, p0, u, v);
    }

    // warm bulb floating above
    float bp[3] = {0.0f, 4.4f, 2.6f};
    ne_add_sphere(e, mat_bulb, bp, 0.30f);

    // ---------------- environment ----------------
    float top[3] = {0.25f, 0.45f, 0.85f};
    float hor[3] = {0.90f, 0.72f, 0.58f};
    float sd[3]  = {0.42f, 0.55f, 0.28f};
    float sc[3]  = {1.0f, 0.90f, 0.78f};
    ne_set_sky(e, top, hor, sd, sc, 4.5f, 1.5f, 1.0f);

    ne_set_max_bounces(e, 5);
    set_showcase_camera(e, 0.0f);
}

} // namespace ne

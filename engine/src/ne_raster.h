// ============================================================================
//  Native Engine — ne_raster.h
//  Real-time software rasterizer: turns the SAME scene the path tracer uses
//  into a 60 fps shaded framebuffer (z-buffer, per-vertex lighting from the
//  scene's sun/sky/emissive materials, distance fog, gamma).
//  No GPU APIs — pure CPU, dependency-free, thread-portable.
// ============================================================================
#pragma once

#include "ne_scene.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdint>

#define NE_MAX(a,b) ((a) > (b) ? (a) : (b))
#define NE_MIN(a,b) ((a) < (b) ? (a) : (b))

namespace ne {

struct RasterTri {
    // clip/projected screen-space data filled during setup
    float sx[3], sy[3], iz[3];   // screen x/y, 1/z
    float r[3], g[3], b[3];      // per-vertex linear color
};

class Raster {
public:
    int width = 0, height = 0;
    std::vector<float> fb;    // linear rgb
    std::vector<float> zbuf;  // depth
    std::vector<uint8_t> rgba;

    void resize(int w, int h) {
        width = w; height = h;
        fb.assign((size_t)w * h * 3, 0.0f);
        zbuf.assign((size_t)w * h, 0.0f);
        rgba.assign((size_t)w * h * 4, 0);
    }

    // ------------------------------------------------------------- render ---
    void render(const Scene& sc, const Vec3& eye, const Vec3& at, float fov_y_deg) {
        if (!width) return;
        build_view(eye, at, fov_y_deg);
        std::memset(fb.data(), 0, fb.size() * sizeof(float));
        std::memset(zbuf.data(), 0, zbuf.size() * sizeof(float));
        draw_sky(sc, eye);
        draw_objects(sc, eye);
        tonemap();
    }

    // View basis / projection --------------------------------------------------
    Vec3 fwd, right, upv;
    float tan_half = 1.0f, aspect = 1.0f;

    void build_view(const Vec3& eye, const Vec3& at, float fov) {
        fwd = normalize(at - eye);
        if (std::fabs(fwd.y) > 0.999f) fwd = Vec3(fwd.x, 0, fwd.z);
        right = normalize(cross(fwd, Vec3(0, 1, 0)));
        upv = cross(right, fwd);
        tan_half = std::tan(fov * NE_PI / 360.0f);
        aspect = (float)width / (float)height;
    }

    float to_screen(const Vec3& p, float& sx, float& sy, float& iz) const {
        Vec3 d = p - eye;
        float z = dot(d, fwd);
        float x = dot(d, right), y = dot(d, upv);
        sx = (0.5f + 0.5f * x / (z * tan_half * aspect)) * width;
        sy = (0.5f - 0.5f * y / (z * tan_half)) * height;
        iz = 1.0f / z;
    }

    // ----------------------------------------------------------- shading ----
    Vec3 shade(const Vec3& p, const Vec3& n, const Material& m, const Scene& sc,
               bool front = true) const {
        Vec3 nn = front ? n : -n;
        float ndl = NE_MAX(0.0f, dot(nn, sc.sky.sun_dir));
        float hemi = 0.5f * (nn.y + 1.0f);
        Vec3 amb = lerp(sc.sky.horizon, sc.sky.top, hemi) * (0.30f * sc.sky.intensity);
        Vec3 sun = sc.sky.sun_color * (sc.sky.sun_intensity * 0.28f);
        // fake specular for metals
        float spec = 0.0f;
        if (m.metallic > 0.2f) {
            Vec3 h = normalize(sc.sky.sun_dir + normalize(eye - p));
            float e = NE_MAX(0.0f, dot(nn, h));
            float rough = NE_MAX(0.05f, m.roughness);
            spec = std::pow(e, 2.0f / (rough * rough)) * m.metallic * 0.9f;
        }
        Vec3 c = m.albedo * (amb + sun * ndl);
        c += Vec3(spec) * sc.sky.sun_color;
        c += m.emissive * 0.16f; // emissive objects glow through the fog a bit
        return c;
    }

    // ---------------------------------------------------------- triangles ---
    void add_lit_tri(Scene const& sc, const Vec3& a, const Vec3& b, const Vec3& c,
                     const Vec3& na, const Vec3& nb, const Vec3& nc,
                     const Material& m) {
        Vec3 e1 = b - a, e2 = c - a;
        Vec3 g = cross(e1, e2);
        float gl = length(g);
        if (gl < 1e-12f) return;
        // per-vertex color + normal, two-sided
        const Vec3* ps[3] = {&a, &b, &c};
        const Vec3* ns[3] = {&na, &nb, &nc};
        Vec3 cols[3]; Vec3 nrm[3];
        for (int i = 0; i < 3; ++i) {
            bool front = dot(*ns[i], eye - *ps[i]) > 0.0f;
            nrm[i] = front ? *ns[i] : -*ns[i];
            cols[i] = shade(*ps[i], nrm[i], m, sc, true);
        }
        // clip against camera-space near plane z > NEARZ
        const float NEARZ = 0.15f;
        struct CV { Vec3 p, n, col; float z; };
        CV in_[3];
        for (int i = 0; i < 3; ++i) {
            Vec3 d = *ps[i] - eye;
            in_[i] = {*ps[i], nrm[i], cols[i], dot(d, fwd)};
        }
        CV poly[8]; int np = 0;
        for (int i = 0; i < 3; ++i) {
            const CV& A = in_[i];
            const CV& B = in_[(i + 1) % 3];
            bool ain = A.z > NEARZ, bin_ = B.z > NEARZ;
            if (ain) poly[np++] = A;
            if (ain != bin_) {
                float t = (NEARZ - A.z) / (B.z - A.z);
                CV C;
                C.p = lerp(A.p, B.p, t);
                C.n = normalize(lerp(A.n, B.n, t) + Vec3(1e-9f));
                C.col = lerp(A.col, B.col, t);
                C.z = NEARZ;
                poly[np++] = C;
            }
        }
        for (int i = 2; i < np; ++i) {
            RasterTri t;
            const CV* v[3] = {&poly[0], &poly[i - 1], &poly[i]};
            for (int k = 0; k < 3; ++k) {
                Vec3 d = v[k]->p - eye;
                float z = NE_MAX(v[k]->z, NEARZ);
                float x = dot(d, right), y = dot(d, upv);
                t.sx[k] = (0.5f + 0.5f * x / (z * tan_half * aspect)) * width;
                t.sy[k] = (0.5f - 0.5f * y / (z * tan_half)) * height;
                t.iz[k] = 1.0f / z;
                t.r[k] = v[k]->col.x; t.g[k] = v[k]->col.y; t.b[k] = v[k]->col.z;
            }
            float area = (t.sx[1] - t.sx[0]) * (t.sy[2] - t.sy[0]) -
                         (t.sy[1] - t.sy[0]) * (t.sx[2] - t.sx[0]);
            if (std::fabs(area) < 1e-4f) continue;
            rasterize(t);
        }
    }

    void rasterize(const RasterTri& t) {
        float minx = NE_MAX(0.0f, NE_MIN(t.sx[0], NE_MIN(t.sx[1], t.sx[2])));
        float maxx = NE_MIN((float)width - 1, NE_MAX(t.sx[0], NE_MAX(t.sx[1], t.sx[2])));
        float miny = NE_MAX(0.0f, NE_MIN(t.sy[0], NE_MIN(t.sy[1], t.sy[2])));
        float maxy = NE_MIN((float)height - 1, NE_MAX(t.sy[0], NE_MAX(t.sy[1], t.sy[2])));
        if (minx > maxx || miny > maxy) return;
        // backface cull for closed solids: rely on area sign being consistent; we
        // shade two-sided instead (quads/planes benefit, solids pay little).
        const float e01x = t.sy[1] - t.sy[0], e01y = -(t.sx[1] - t.sx[0]);
        const float e12x = t.sy[2] - t.sy[1], e12y = -(t.sx[2] - t.sx[1]);
        const float e20x = t.sy[0] - t.sy[2], e20y = -(t.sx[0] - t.sx[2]);
        auto edge = [&](float ax, float ay, float bx, float by, float px, float py) {
            float v = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            return v;
        };
        float s = edge(t.sx[0], t.sy[0], t.sx[1], t.sy[1], t.sx[2], t.sy[2]);
        float inv_s = s != 0 ? 1.0f / s : 0;

        int x0 = (int)std::floor(minx), x1 = (int)std::ceil(maxx);
        int y0 = (int)std::floor(miny), y1 = (int)std::ceil(maxy);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                // inclusive edge tests: shared edges rasterize on both triangles
                // (kills hairline seams; harmless where colors meet)
                float w0 = edge(t.sx[1], t.sy[1], t.sx[2], t.sy[2], px, py);
                if (s > 0 ? w0 < -1e-3f : w0 > 1e-3f) continue;
                float w1 = edge(t.sx[2], t.sy[2], t.sx[0], t.sy[0], px, py);
                if (s > 0 ? w1 < -1e-3f : w1 > 1e-3f) continue;
                float w2 = edge(t.sx[0], t.sy[0], t.sx[1], t.sy[1], px, py);
                if (s > 0 ? w2 < -1e-3f : w2 > 1e-3f) continue;
                float b0 = w0 * inv_s, b1 = w1 * inv_s, b2 = w2 * inv_s;
                // perspective-correct
                float iz = t.iz[0] * b0 + t.iz[1] * b1 + t.iz[2] * b2;
                if (iz <= 0) continue;
                float z = 1.0f / iz;
                size_t idx = (size_t)y * width + x;
                if (zbuf[idx] > z) continue;
                zbuf[idx] = z;
                float zr = (t.r[0] * b0 * t.iz[0] + t.r[1] * b1 * t.iz[1] + t.r[2] * b2 * t.iz[2]) / iz;
                float zg = (t.g[0] * b0 * t.iz[0] + t.g[1] * b1 * t.iz[1] + t.g[2] * b2 * t.iz[2]) / iz;
                float zb = (t.b[0] * b0 * t.iz[0] + t.b[1] * b1 * t.iz[1] + t.b[2] * b2 * t.iz[2]) / iz;
                // fog
                float fogf = NE_MIN(1.0f, NE_MAX(0.0f, (z - 30.0f) / 90.0f));
                Vec3 fogc = sky_horizon_color;
                zr = zr * (1 - fogf) + fogc.x * fogf;
                zg = zg * (1 - fogf) + fogc.y * fogf;
                zb = zb * (1 - fogf) + fogc.z * fogf;
                float* o = &fb[idx * 3];
                o[0] = zr; o[1] = zg; o[2] = zb;
            }
        }
    }

    Vec3 sky_horizon_color;

    // ------------------------------------------------------------- sky ------
    void draw_sky(const Scene& sc, const Vec3& eye_) {
        eye = eye_; // shading() uses it; stored here for convenience
        sky_horizon_color = sc.sky.horizon * (0.75f * sc.sky.intensity);
        // vertical gradient (row table) — cheap
        std::vector<Vec3> rowcol(height);
        for (int y = 0; y < height; ++y)
            rowcol[y] = sc.sky.radiance(normalize(fwd + upv * ((0.5f - (float)y / (float)height) * 1.4f)));
        for (int y = 0; y < height; ++y) {
            const Vec3 c = rowcol[y];
            float* o = &fb[(size_t)y * width * 3];
            for (int x = 0; x < width; ++x) { *o++ = c.x; *o++ = c.y; *o++ = c.z; }
        }
    }

    // ---------------------------------------------------------- objects -----
    static void sphere_mesh(std::vector<Vec3>& V, std::vector<Vec3>& N, std::vector<uint32_t>& I,
                            int segs, int rings) {
        if (!V.empty()) return;
        for (int r = 0; r <= rings; ++r) {
            float phi = NE_PI * r / rings;
            for (int s = 0; s <= segs; ++s) {
                float th = 2.0f * NE_PI * s / segs;
                Vec3 n(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
                V.push_back(n); N.push_back(n);
            }
        }
        for (int r = 0; r < rings; ++r)
            for (int s = 0; s < segs; ++s) {
                int a = r * (segs + 1) + s, b = a + segs + 1;
                uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
                I.insert(I.end(), {ua, ub, ua + 1, ub, ub + 1, ua + 1});
            }
    }

    void draw_objects(const Scene& sc, const Vec3& eye_) {
        eye = eye_;
        static std::vector<Vec3> SV, SN;
        static std::vector<uint32_t> SI;
        sphere_mesh(SV, SN, SI, 18, 12);

        for (const Object& o : sc.objects) {
            if (o.mat < 0 || o.mat >= (int)sc.materials.size()) continue;
            const Material& m = sc.materials[o.mat];
            switch (o.type) {
            case ObjType::Sphere: {
                if (o.radius <= 0.0005f) break;
                for (size_t i = 0; i < SI.size(); i += 3) {
                    auto mk = [&](uint32_t vi, Vec3& p, Vec3& n) {
                        p = o.pos + SV[vi] * o.radius;
                        n = SN[vi];
                    };
                    Vec3 pa, na, pb, nb, pc, nc;
                    mk(SI[i], pa, na); mk(SI[i + 1], pb, nb); mk(SI[i + 2], pc, nc);
                    add_lit_tri(sc, pa, pb, pc, na, nb, nc, m);
                }
                break;
            }
            case ObjType::Box: {
                float a = o.rot_y * NE_PI / 180.0f;
                float ca = std::cos(a), sa = std::sin(a);
                auto xf = [&](Vec3 v) {
                    Vec3 r(v.x * ca + v.z * sa, v.y, -v.x * sa + v.z * ca);
                    return r + o.pos;
                };
                Vec3 ex(o.half.x, 0, 0), ey(0, o.half.y, 0), ez(0, 0, o.half.z);
                Vec3 n[6] = { Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1) };
                Vec3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    Vec3 v = o.pos + ex * ((i & 1) ? 1 : -1) + ey * ((i & 2) ? 1 : -1) + ez * ((i & 4) ? 1 : -1);
                    corners[i] = xf(v - o.pos);
                }
                int fidx[6][4] = {{0,2,6,4},{1,5,7,3},{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6}};
                for (int f = 0; f < 6; ++f) {
                    const int* q = fidx[f];
                    add_lit_tri(sc, corners[q[0]], corners[q[1]], corners[q[2]], n[f], n[f], n[f], m);
                    add_lit_tri(sc, corners[q[0]], corners[q[2]], corners[q[3]], n[f], n[f], n[f], m);
                }
                break;
            }
            case ObjType::Quad: {
                Vec3 nn = o.q_normal;
                add_lit_tri(sc, o.q_p0, o.q_p0 + o.q_u, o.q_p0 + o.q_u + o.q_v, nn, nn, nn, m);
                add_lit_tri(sc, o.q_p0, o.q_p0 + o.q_u + o.q_v, o.q_p0 + o.q_v, nn, nn, nn, m);
                break;
            }
            case ObjType::Plane: {
                // adaptive grid so checker patterns & shadows-ish look sane
                float cell = m.checker > 0 ? m.checker : 2.0f;
                int R = 20;
                Vec3 center = eye; center.y = 0;
                float cx = std::round(center.x / cell) * cell;
                float cz = std::round(center.z / cell) * cell;
                float h = (float)R * cell;
                Material gm = m;
                const Vec3 hi = m.albedo, lo = m.albedo * 0.35f;
                const Vec3 mean = (hi + lo) * 0.5f;
                for (int ix = -R; ix < R; ++ix)
                    for (int iz = -R; iz < R; ++iz) {
                        float x0 = cx + ix * cell, z0 = cz + iz * cell;
                        Vec3 p[4] = {{x0, 0, z0}, {x0 + cell, 0, z0}, {x0 + cell, 0, z0 + cell}, {x0, 0, z0 + cell}};
                        Vec3 nn(0, 1, 0);
                        // checker tint per cell with distance-based mipmap fade
                        if (m.checker > 0) {
                            bool alt = ((ix + iz) & 1) != 0;
                            Vec3 col = alt ? lo : hi;
                            float dx = x0 + cell * 0.5f - eye.x, dz = z0 + cell * 0.5f - eye.z;
                            float d = std::sqrt(dx * dx + dz * dz);
                            float fade = clampf((d - 10.0f) / 26.0f, 0.0f, 1.0f);
                            col = lerp(col, mean, fade);
                            gm.albedo = col;
                        }
                        add_lit_tri(sc, p[0], p[1], p[2], nn, nn, nn, gm);
                        add_lit_tri(sc, p[0], p[2], p[3], nn, nn, nn, gm);
                    }
                break;
            }
            case ObjType::Mesh: {
                if (o.local_verts.empty() || o.indices.empty()) break;
                size_t vn = o.local_verts.size() / 3;
                std::vector<Vec3> W(vn);
                Mat4 rot = Mat4::rot_y(o.rot_y);
                for (size_t i = 0; i < vn; ++i) {
                    Vec3 v(o.local_verts[i * 3], o.local_verts[i * 3 + 1], o.local_verts[i * 3 + 2]);
                    W[i] = rot.transform_point(v) + o.pos; // matches Scene::rebuild_mesh
                }
                for (size_t i = 0; i + 2 < o.indices.size(); i += 3) {
                    Vec3 a = W[o.indices[i]], b = W[o.indices[i + 1]], c = W[o.indices[i + 2]];
                    Vec3 nn = normalize(cross(b - a, c - a));
                    add_lit_tri(sc, a, b, c, nn, nn, nn, m);
                }
                break;
            }
            }
        }
    }

    Vec3 eye;

    // ------------------------------------------------------------ present ---
    static float to_srgb(float x) {
        x = x < 0 ? 0 : x;
        x = x > 1 ? 1 : x;
        return x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1 / 2.4f) - 0.055f;
    }

    void tonemap() {
        rgba.resize(fb.size() / 3 * 4);
        for (size_t i = 0, j = 0; i < fb.size(); i += 3, j += 4) {
            float r = fb[i], g = fb[i + 1], b = fb[i + 2];
            // filmic-ish soft knee
            r = r / (1.0f + r); g = g / (1.0f + g); b = b / (1.0f + b);
            rgba[j]     = (uint8_t)(to_srgb(r * 1.35f) * 255.0f + 0.5f);
            rgba[j + 1] = (uint8_t)(to_srgb(g * 1.35f) * 255.0f + 0.5f);
            rgba[j + 2] = (uint8_t)(to_srgb(b * 1.35f) * 255.0f + 0.5f);
            rgba[j + 3] = 255;
        }
    }

    // ------------------------------------------------------------- HUD ------
    void text(int x, int y, const char* s, int scale, uint32_t color);
};

// 3x5 pixel font (bit rows top->bottom), printable subset.
inline const char* raster_glyph(char c) {
    switch (c) {
    case '0': return "111101101101111"; case '1': return "010110010010111";
    case '2': return "111001111100111"; case '3': return "111001111001111";
    case '4': return "101101111001001"; case '5': return "111100111001111";
    case '6': return "111100111101111"; case '7': return "111001010010010";
    case '8': return "111101111101111"; case '9': return "111101111001111";
    case 'A': return "010101111101101"; case 'B': return "110101110101110";
    case 'C': return "011100100100011"; case 'D': return "110101101101110";
    case 'E': return "111100110100111"; case 'F': return "111100110100100";
    case 'G': return "011100101101111"; case 'H': return "101101111101101";
    case 'I': return "111010010010111"; case 'J': return "001001001101110";
    case 'K': return "101101110101101"; case 'L': return "100100100100111";
    case 'M': return "101111111101101"; case 'N': return "101111111111101";
    case 'O': return "111101101101111"; case 'P': return "111101111100100";
    case 'Q': return "111101101111001"; case 'R': return "111101110101101";
    case 'S': return "011100010001110"; case 'T': return "111010010010010";
    case 'U': return "101101101101111"; case 'V': return "101101101010010";
    case 'W': return "101101111111101"; case 'X': return "101101010101101";
    case 'Y': return "101101111010010"; case 'Z': return "111001010100111";
    case ' ': return "000000000000000"; case ':': return "000010000010000";
    case '.': return "000000000000110"; case ',': return "000000000010100";
    case '-': return "000000111000000"; case '+': return "000010111010000";
    case '/': return "001001010100100"; case '!': return "010010010000010";
    case '(': return "001010010010001"; case ')': return "100010010010100";
    case '%': return "101001010100101"; case '?': return "111001011000010";
    case '#': return "010111010111010"; case '_': return "000000000000111";
    case '\'': return "010010000000000"; case '*': return "000101111101000";
    }
    return "000000000000000";
}

inline void Raster::text(int x, int y, const char* s, int scale, uint32_t color) {
    uint8_t cr = (color >> 16) & 0xFF, cg = (color >> 8) & 0xFF, cb = color & 0xFF;
    if (scale < 1) scale = 1;
    for (; *s; ++s) {
        if (*s == ' ') { x += 4 * scale; continue; }
        const char* g = raster_glyph(*s);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (g[row * 3 + col] == '1') {
                    for (int sy = 0; sy < scale; ++sy)
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = x + col * scale + sx, py = y + row * scale + sy;
                            if (px < 0 || py < 0 || px >= width || py >= height) continue;
                            size_t j = ((size_t)py * width + px) * 4;
                            rgba[j] = cr; rgba[j + 1] = cg; rgba[j + 2] = cb;
                        }
                }
        x += 4 * scale;
    }
}

} // namespace ne

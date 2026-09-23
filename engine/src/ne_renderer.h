// ============================================================================
//  Native Engine — ne_renderer.h / renderer core
//  Multithreaded progressive path tracer:
//    - ray-traced global illumination (diffuse interreflection)
//    - ray-traced soft shadows + directional sun (next-event estimation)
//    - ray-traced reflections (metals, glossy Fresnel plastics)
//    - refraction / glass with total internal reflection
//    - emissive area lights (sphere / quad / box)
//    - procedural sky with sun disk
//    - depth of field, anti-aliasing, ACES tonemapping
// ============================================================================
#pragma once

#include "ne_scene.h"
#include "ne_threads.h"
#include <atomic>
#include <vector>
#include <cstring>
#include <chrono>

namespace ne {

inline std::vector<uint8_t> encode_png(const uint8_t* rgba, int w, int h);

struct Stats {
    double last_ms = 0.0;
    double last_mrays = 0.0; // million rays per second
    int accum_samples = 0;
};

class Renderer {
public:
    Renderer(int w, int h) { resize(w, h); }

    void resize(int w, int h) {
        ne::SpinGuard lk(m_frame);
        width = w; height = h;
        accum.assign((size_t)w * h * 3, 0.0f);
        rgba.assign((size_t)w * h * 4, 0);
        png.clear();
        png_dirty = true;
        reset_accum();
    }

    void reset_accum() { accum_samples = 0; }

    Scene scene;
    Stats stats;
    bool streaming = true; // true: each render() restarts accumulation (animation)

    // Render `spp` samples per pixel (adds to accumulation when streaming=false).
    // Returns elapsed milliseconds.
    double render(int spp) {
        auto t0 = std::chrono::steady_clock::now();
        if (streaming) reset_accum();

        scene.ray_count.store(0, std::memory_order_relaxed);
        int tiles_x = (width + TILE - 1) / TILE;
        int tiles_y = (height + TILE - 1) / TILE;
        tile_counter.store(0);
        tiles_total = tiles_x * tiles_y;
        frame_seed.fetch_add(0x9e3779b9u);

        unsigned nthreads = ne::hardware_threads();
        std::vector<ne::thread_handle> threads;
        threads.reserve(nthreads);
        for (unsigned i = 1; i < nthreads; ++i)
            threads.push_back(ne::spawn_thread(&Renderer::thread_entry,
                                               new WorkerArg{this, spp}));
        worker(spp);  // calling thread takes tiles too
        for (auto& t : threads) ne::join_thread(t);

        accum_samples += spp;
        double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        uint64_t rays = scene.ray_count.load();
        stats.last_ms = ms;
        stats.last_mrays = ms > 0 ? (double)rays / (ms * 1000.0) : 0.0;
        stats.accum_samples = accum_samples;

        present();
        return ms;
    }

    // Copy tonemapped RGBA8 + PNG out (thread-safe with render).
    void get_rgba(std::vector<uint8_t>& out) { ne::SpinGuard lk(m_frame); out = rgba; }
    void get_png(std::vector<uint8_t>& out) {
        ne::SpinGuard lk(m_frame);
        if (png_dirty) { png = encode_png(rgba.data(), width, height); png_dirty = false; }
        out = png;
    }

private:
    static const int TILE = 32;
    int width = 0, height = 0;
    std::vector<float> accum;
    std::vector<uint8_t> rgba;
    std::vector<uint8_t> png;
    bool png_dirty = true;
    int accum_samples = 0;
    std::atomic<uint32_t> tile_counter{0};
    std::atomic<uint32_t> frame_seed{0x1234567u};
    uint32_t tiles_total = 0;
    ne::SpinLock m_frame;

    // ------------------------------------------------------------- trace ----
    Vec3 shade_diffuse_direct(const Vec3& p, const Vec3& n, const Vec3& albedo, RNG& rng) const {
        Vec3 L(0.0f);
        Vec3 shadow_o = p + n * 1e-3f;
        // --- sun (directional, cone-sampled) ---
        {
            Vec3 sdir = sample_cone(scene.sky.sun_dir, scene.sky.sun_cos_angle, rng);
            float ndl = dot(n, sdir);
            if (ndl > 0.0f) {
                float T = scene.shadow_transmittance(shadow_o, sdir, 1e30f);
                if (T > 0.0f)
                    L += (albedo / NE_PI) * scene.sky.sun_color * (scene.sky.sun_intensity * ndl * T);
            }
        }
        // --- one random emissive area light (uniform surface sampling) ---
        size_t nl = scene.lights.size();
        if (nl > 0) {
            int obj = scene.lights[(size_t)(rng.next_float() * nl) % nl];
            Vec3 lp, ln;
            float area;
            Vec3 le = scene.sample_light(obj, rng, lp, ln, area);
            Vec3 wi = lp - p;
            float d2 = length_sq(wi);
            float dist = std::sqrt(d2);
            wi /= dist;
            float cos_s = dot(n, wi);
            float cos_l = dot(ln, -wi);
            if (cos_s > 0.0f && cos_l > 0.0f) {
                float T = scene.shadow_transmittance(shadow_o, wi, dist - 2e-3f);
                if (T > 0.0f) {
                    // pdf = 1/(area*nl); contribution = (albedo/pi) * Le * G / pdf
                    L += (albedo / NE_PI) * le * (cos_s * cos_l / d2 * area * (float)nl * T);
                }
            }
        }
        return L;
    }

    Vec3 trace(Vec3 o, Vec3 d, RNG& rng) const {
        Vec3 beta(1.0f), L(0.0f);
        bool specular = true; // first hit always shows emission directly

        for (int bounce = 0; bounce < scene.max_bounces; ++bounce) {
            Hit hit;
            if (!scene.intersect(o, d, hit)) {
                L += beta * scene.sky.radiance(d);
                break;
            }
            const Material& m = scene.materials[hit.mat];

            // Emission: seen directly or via specular chains (NEE covers diffuse)
            if (specular) L += beta * m.emissive;

            // Albedo with optional procedural checker
            Vec3 albedo = m.albedo;
            if (m.checker > 0.0f) {
                float cx = std::floor(hit.p.x / m.checker);
                float cz = std::floor(hit.p.z / m.checker);
                float cy = std::floor(hit.p.y / m.checker);
                float k = cx + cz + cy;
                bool even = (std::fmod(std::fabs(k), 2.0f) < 1.0f);
                if (!even) albedo *= 0.35f;
            }

            // Face the shading normal toward the incoming ray. This makes the
            // surface two-sided: meshes with inverted winding and backfaces
            // still shade and reflect correctly.
            Vec3 geo_n = hit.n;
            bool front_face = dot(d, geo_n) < 0.0f;
            Vec3 n = front_face ? geo_n : -geo_n;
            float cos_i = clampf(-dot(d, n), 0.0f, 1.0f);

            // ---------------- glass ----------------
            if (m.transmission > 0.0f) {
                bool entering = front_face;
                Vec3 nf = n; // already faces the ray
                float cos_face = cos_i;
                float eta = entering ? 1.0f / m.ior : m.ior;
                float F = schlick(cos_face, m.ior);
                Vec3 refr = refract(d, nf, eta);
                if (length_sq(refr) < 1e-6f) F = 1.0f; // TIR
                if (rng.next_float() < F) {
                    d = normalize(reflect(d, nf));
                    o = hit.p + nf * 1e-3f;
                } else {
                    d = normalize(refr);
                    o = hit.p - nf * 1e-3f;
                }
                beta = beta * albedo;
                specular = true;
                continue;
            }

            // ---------------- metal / plastic ----------------
            bool metal = m.metallic > 0.5f;
            // Fresnel-weighted choice: metals always reflect; plastics reflect
            // with Schlick probability (F0 = 4%) for a glossy dielectric sheen.
            float spec_prob = metal ? 1.0f : (0.04f + 0.96f * std::pow(1.0f - cos_i, 5.0f));

            if (rng.next_float() < spec_prob) {
                // specular reflection lobe
                Vec3 r = reflect(d, n);
                float rough = std::max(m.roughness, 0.005f);
                Vec3 perturb = random_in_unit_sphere(rng) * (rough * rough);
                Vec3 nd = normalize(r + perturb);
                if (dot(nd, n) < 0.0f) nd = reflect(nd, n);
                d = normalize(nd);
                o = hit.p + n * 1e-3f;
                beta = beta * (m.metallic > 0.5f ? albedo : Vec3(1.0f));
                specular = true;
            } else {
                // diffuse lobe: direct lighting via NEE + indirect via cosine ray
                Vec3 nd = cosine_hemisphere(n, rng);
                L += beta * shade_diffuse_direct(hit.p + n * 1e-3f, n, albedo, rng);
                d = nd;
                o = hit.p + n * 1e-3f;
                beta = beta * albedo;
                specular = false;
            }

            // Russian roulette
            if (bounce >= 2) {
                float q = std::max(beta.x, std::max(beta.y, beta.z));
                if (rng.next_float() > q) break;
                beta /= std::max(q, 0.05f);
            }
        }
        // gentle firefly clamp (removes sun-disk speckles, negligible bias)
        return Vec3(std::min(L.x, 40.0f), std::min(L.y, 40.0f), std::min(L.z, 40.0f));
    }

    struct WorkerArg { Renderer* r; int spp; };

    static NE_THREAD_ENTRY thread_entry(void* p) {

        WorkerArg* a = static_cast<WorkerArg*>(p);

        a->r->worker(a->spp);

        delete a;
#ifdef _WIN32

        return 0;
#else

        return;
#endif

    }


    void worker(int spp) {
        uint32_t seed = frame_seed.load() ^ (uint32_t)(uintptr_t)this;
        std::vector<float> tile(TILE * TILE * 3);
        for (;;) {
            uint32_t tile_id = tile_counter.fetch_add(1);
            if (tile_id >= tiles_total) break;
            int tx = (int)(tile_id % ((width + TILE - 1) / TILE));
            int ty = (int)(tile_id / ((width + TILE - 1) / TILE));
            RNG rng;
            rng.seed(((uint64_t)tile_id << 20) ^ seed, 0xda3e39cb94b95bdbull);

            int x0 = tx * TILE, y0 = ty * TILE;
            int x1 = std::min(x0 + TILE, width), y1 = std::min(y0 + TILE, height);

            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    Vec3 sum(0.0f);
                    for (int s = 0; s < spp; ++s) {
                        float jx = rng.next_float(), jy = rng.next_float();
                        Vec3 o, d;
                        scene.camera.make_ray(x + jx, y + jy, width, height, rng, o, d);
                        sum += trace(o, d, rng);
                    }
                    // store raw sums: present() divides by the running total
                    int li = ((y - y0) * TILE + (x - x0)) * 3;
                    tile[li] = sum.x; tile[li + 1] = sum.y; tile[li + 2] = sum.z;
                }
            }
            // merge tile into accumulation buffer (tiles are disjoint -> safe)
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    int li = ((y - y0) * TILE + (x - x0)) * 3;
                    size_t gi = ((size_t)y * width + x) * 3;
                    accum[gi] += tile[li];
                    accum[gi + 1] += tile[li + 1];
                    accum[gi + 2] += tile[li + 2];
                }
        }
    }

    // ------------------------------------------------- tonemap + present ----
    static NE_INLINE float aces(float x) {
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        return clampf((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
    }

    void present() {
        ne::SpinGuard lk(m_frame);
        float inv = 1.0f / std::max(1, accum_samples);
        for (size_t i = 0, j = 0; i < accum.size(); i += 3, j += 4) {
            float r = accum[i] * inv, g = accum[i + 1] * inv, b = accum[i + 2] * inv;
            rgba[j]     = (uint8_t)(std::pow(aces(r), 1.0f / 2.2f) * 255.0f + 0.5f);
            rgba[j + 1] = (uint8_t)(std::pow(aces(g), 1.0f / 2.2f) * 255.0f + 0.5f);
            rgba[j + 2] = (uint8_t)(std::pow(aces(b), 1.0f / 2.2f) * 255.0f + 0.5f);
            rgba[j + 3] = 255;
        }
        png_dirty = true;
    }
};

// ---------------------------------------------------------------------------
//  Minimal dependency-free PNG encoder (8-bit RGBA, fixed-Huffman DEFLATE
//  with run-length matching — excellent on rendered images, tiny code).
// ---------------------------------------------------------------------------
inline std::vector<uint8_t> encode_png(const uint8_t* rgba, int w, int h);

namespace png_enc {

struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t bitbuf = 0;
    int bitcount = 0;
    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}

    inline void put(uint32_t value, int bits) { // LSB-first
        bitbuf |= value << bitcount;
        bitcount += bits;
        while (bitcount >= 8) {
            out.push_back((uint8_t)(bitbuf & 0xFF));
            bitbuf >>= 8;
            bitcount -= 8;
        }
    }
    inline void put_code(uint32_t code, int bits) { // Huffman: MSB-first
        // reverse the code bits then write LSB-first
        uint32_t rev = 0;
        for (int i = 0; i < bits; ++i) rev |= ((code >> i) & 1u) << (bits - 1 - i);
        put(rev, bits);
    }
    inline void flush() {
        if (bitcount > 0) { out.push_back((uint8_t)(bitbuf & 0xFF)); bitbuf = 0; bitcount = 0; }
    }
};

inline void write_literal(BitWriter& bw, int sym) {
    if (sym <= 143) bw.put_code(0x30u + (uint32_t)sym, 8);
    else bw.put_code(0x190u + (uint32_t)(sym - 144), 9);
}
inline void write_length_code(BitWriter& bw, int sym) {
    if (sym <= 279) bw.put_code((uint32_t)(sym - 256), 7);
    else bw.put_code(0xC0u + (uint32_t)(sym - 280), 8);
}

// Length code tables (DEFLATE RFC 1951).
struct LenCode { int sym; int extra_bits; int base; };
inline const LenCode* length_table() {
    static const LenCode t[28] = {
        {257,0,3},{258,0,4},{259,0,5},{260,0,6},{261,0,7},{262,0,8},{263,0,9},{264,0,10},
        {265,1,11},{266,1,13},{267,1,15},{268,1,17},
        {269,2,19},{270,2,23},{271,2,27},{272,2,31},
        {273,3,35},{274,3,43},{275,3,51},{276,3,59},
        {277,4,67},{278,4,83},{279,4,99},{280,4,115},
        {281,5,131},{282,5,163},{283,5,195},{284,5,227}};
    return t;
}
inline int length_code_for(int len, int& extra_bits, int& extra_val) {
    const LenCode* t = length_table();
    int i = 27;
    while (i > 0 && t[i].base > len) --i;
    // clamp: len 258 uses code 285 (no extra bits); code 284 covers up to 257
    if (len >= 258) { extra_bits = 0; extra_val = 0; return 285; }
    extra_bits = t[i].extra_bits;
    extra_val = len - t[i].base;
    return t[i].sym;
}

} // namespace png_enc

inline std::vector<uint8_t> encode_png(const uint8_t* rgba, int w, int h) {
    using namespace png_enc;
    auto crc_table = []{
        static uint32_t table[256];
        static bool init = false;
        if (!init) {
            for (uint32_t n = 0; n < 256; ++n) {
                uint32_t c = n;
                for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                table[n] = c;
            }
            init = true;
        }
        return table;
    }();
    auto crc32 = [&](const uint8_t* data, size_t len) {
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    auto adler32 = [&](const uint8_t* data, size_t len) {
        uint32_t a = 1, b = 0;
        for (size_t i = 0; i < len; ++i) { a = (a + data[i]) % 65521; b = (b + a) % 65521; }
        return (b << 16) | a;
    };
    auto push32 = [](std::vector<uint8_t>& v, uint32_t x) {
        v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
        v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
    };
    auto chunk = [&](std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
        push32(out, (uint32_t)data.size());
        std::vector<uint8_t> body;
        for (int i = 0; i < 4; ++i) body.push_back((uint8_t)type[i]);
        body.insert(body.end(), data.begin(), data.end());
        out.insert(out.end(), body.begin(), body.end());
        push32(out, crc32(body.data(), body.size()));
    };

    std::vector<uint8_t> out;
    out.insert(out.end(), {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'});
    std::vector<uint8_t> ihdr;
    push32(ihdr, (uint32_t)w); push32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0);
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);

    // ---- filtered raw scanlines: filter 0 for the first row, 2 (Up) after ----
    size_t stride = (size_t)w * 4;
    std::vector<uint8_t> raw((size_t)(stride + 1) * h);
    for (int y = 0; y < h; ++y) {
        uint8_t* dst = &raw[(size_t)y * (stride + 1)];
        dst[0] = y == 0 ? 0 : 2;
        const uint8_t* cur = rgba + (size_t)y * stride;
        const uint8_t* prev = cur - stride;
        if (y == 0) std::memcpy(dst + 1, cur, stride);
        else for (size_t i = 0; i < stride; ++i) dst[1 + i] = (uint8_t)(cur[i] - prev[i]);
    }

    // ---- DEFLATE: fixed Huffman, RLE runs of identical bytes -----------------
    std::vector<uint8_t> idat;
    idat.push_back(0x78); idat.push_back(0x01);
    BitWriter bw(idat);
    bw.put(1, 1);  // BFINAL
    bw.put(1, 2);  // BTYPE=01 fixed Huffman
    size_t i = 0;
    const size_t n = raw.size();
    while (i < n) {
        uint8_t v = raw[i];
        size_t run = 1;
        while (i + run < n && raw[i + run] == v && run < 258) ++run;
        if (run >= 4) {
            // literal + (match length run-1 at distance 1) == run copies of v
            write_literal(bw, v);
            int eb = 0, ev = 0;
            int lsym = length_code_for((int)(run - 1), eb, ev);
            write_length_code(bw, lsym);
            if (eb) bw.put((uint32_t)ev, eb);
            bw.put_code(0, 5); // distance code 0 (dist = 1)
            i += run;
        } else {
            write_literal(bw, v);
            i += 1;
        }
    }
    write_length_code(bw, 256); // end of block
    bw.flush();
    push32(idat, adler32(raw.data(), raw.size()));
    chunk(out, "IDAT", idat);
    chunk(out, "IEND", {});
    return out;
}

} // namespace ne

// ============================================================================
//  Native Engine — ne_math.h
//  Vector / matrix math, RNG and sampling utilities.
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

#define NE_INLINE inline
#define NE_PI 3.14159265358979323846f
#define NE_INV_PI 0.31830988618379067154f
#define NE_TAU 6.28318530717958647692f

namespace ne {

// ---------------------------------------------------------------- Vec3 ----
struct Vec3 {
    float x = 0, y = 0, z = 0;
    NE_INLINE Vec3() = default;
    NE_INLINE Vec3(float s) : x(s), y(s), z(s) {}
    NE_INLINE Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    NE_INLINE Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    NE_INLINE Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    NE_INLINE Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    NE_INLINE Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    NE_INLINE Vec3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
    NE_INLINE Vec3 operator-() const { return {-x, -y, -z}; }
    NE_INLINE Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    NE_INLINE Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    NE_INLINE Vec3& operator/=(float s) { float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; return *this; }
};

NE_INLINE Vec3 operator*(float s, const Vec3& v) { return v * s; }
NE_INLINE Vec3 operator/(float s, const Vec3& v) { return Vec3(s / v.x, s / v.y, s / v.z); }

NE_INLINE float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
NE_INLINE Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
NE_INLINE float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
NE_INLINE float length_sq(const Vec3& v) { return dot(v, v); }
NE_INLINE Vec3 normalize(const Vec3& v) { return v / (length(v) + 1e-12f); }
NE_INLINE Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
NE_INLINE float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
NE_INLINE Vec3 reflect(const Vec3& d, const Vec3& n) { return d - n * (2.0f * dot(d, n)); }

// Refract: returns zero vector on total internal reflection.
NE_INLINE Vec3 refract(const Vec3& d, const Vec3& n, float eta) {
    float cosI = -dot(d, n);
    float k = 1.0f - eta * eta * (1.0f - cosI * cosI);
    if (k < 0.0f) return Vec3(0.0f);
    return d * eta + n * (eta * cosI - std::sqrt(k));
}

NE_INLINE float schlick(float cosTheta, float ior) {
    float r0 = (1.0f - ior) / (1.0f + ior);
    r0 *= r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosTheta, 5.0f);
}

// ---------------------------------------------------------------- Mat4 ----
struct Mat4 { // column-major, like OpenGL
    float m[16];
    NE_INLINE Mat4() { std::fill(m, m + 16, 0.0f); m[0] = m[5] = m[10] = m[15] = 1.0f; }

    NE_INLINE Vec3 transform_point(const Vec3& p) const {
        return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    }
    static Mat4 translate(const Vec3& t) {
        Mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
    }
    static Mat4 rot_y(float deg) {
        Mat4 r; float a = deg * NE_PI / 180.0f;
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }
    static Mat4 rot_x(float deg) {
        Mat4 r; float a = deg * NE_PI / 180.0f;
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 mul(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int ro = 0; ro < 4; ++ro) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += a.m[k * 4 + ro] * b.m[c * 4 + k];
                r.m[c * 4 + ro] = s;
            }
        return r;
    }
    NE_INLINE Mat4 operator*(const Mat4& o) const { return mul(*this, o); }
};

NE_INLINE Mat4 rot_y_around(const Vec3& pivot, float deg) {
    return Mat4::translate(pivot) * Mat4::rot_y(deg) * Mat4::translate(-pivot);
}

// ------------------------------------------------------------ AABB --------
struct AABB {
    Vec3 mn = Vec3(1e30f), mx = Vec3(-1e30f);
    void grow(const Vec3& p) {
        mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
        mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
    }
    void grow(const AABB& b) { grow(b.mn); grow(b.mx); }
    NE_INLINE bool is_empty() const { return mn.x > mx.x; }
    NE_INLINE float area() const {
        Vec3 e = mx - mn;
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
    NE_INLINE Vec3 center() const { return (mn + mx) * 0.5f; }
    // Slab test. Returns entry t (>= tmin) or -1.
    NE_INLINE float intersect(const Vec3& o, const Vec3& inv_d, float tmax) const {
        float t0 = (mn.x - o.x) * inv_d.x, t1 = (mx.x - o.x) * inv_d.x;
        float tmin = std::min(t0, t1), tmaxb = std::max(t0, t1);
        t0 = (mn.y - o.y) * inv_d.y; t1 = (mx.y - o.y) * inv_d.y;
        tmin = std::max(tmin, std::min(t0, t1));
        tmaxb = std::min(tmaxb, std::max(t0, t1));
        t0 = (mn.z - o.z) * inv_d.z; t1 = (mx.z - o.z) * inv_d.z;
        tmin = std::max(tmin, std::min(t0, t1));
        tmaxb = std::min(tmaxb, std::max(t0, t1));
        if (tmaxb < std::max(tmin, 0.0f) || tmin > tmax) return -1.0f;
        return tmin < 0.0f ? 0.0f : tmin;
    }
};

// ------------------------------------------------------------- RNG --------
// PCG32 — fast, high quality, deterministic.
struct RNG {
    uint64_t state = 0x853c49e6748fea9bull;
    uint64_t inc = 0xda3e39cb94b95bdbull;

    void seed(uint64_t seq, uint64_t stream) {
        state = 0u; inc = (stream << 1u) | 1u;
        next_uint(); state += seq; next_uint();
    }
    NE_INLINE uint32_t next_uint() {
        uint64_t old = state;
        state = old * 6364136223846793005ull + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }
    NE_INLINE float next_float() { return (float)(next_uint() >> 8u) * (1.0f / 16777216.0f); }
    NE_INLINE Vec3 next_vec3() { return {next_float(), next_float(), next_float()}; }
};

NE_INLINE Vec3 random_in_unit_sphere(RNG& rng) {
    for (int i = 0; i < 16; ++i) {
        Vec3 p = rng.next_vec3() * 2.0f - Vec3(1.0f);
        if (length_sq(p) < 1.0f) return p;
    }
    return Vec3(0, 1, 0);
}

NE_INLINE Vec3 cosine_hemisphere(const Vec3& n, RNG& rng) {
    float r1 = rng.next_float() * NE_TAU;
    float r2 = rng.next_float();
    float r2s = std::sqrt(r2);
    Vec3 w = n;
    Vec3 u = std::fabs(w.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    u = normalize(cross(u, w));
    Vec3 v = cross(w, u);
    Vec3 d = normalize(u * (std::cos(r1) * r2s) + v * (std::sin(r1) * r2s) + w * std::sqrt(1.0f - r2));
    return d;
}

NE_INLINE Vec3 sample_cone(const Vec3& dir, float cos_theta_max, RNG& rng) {
    float r1 = rng.next_float(), r2 = rng.next_float();
    float cos_t = 1.0f - r1 * (1.0f - cos_theta_max);
    float sin_t = std::sqrt(std::max(0.0f, 1.0f - cos_t * cos_t));
    float phi = r2 * NE_TAU;
    Vec3 w = dir;
    Vec3 u = std::fabs(w.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    u = normalize(cross(u, w));
    Vec3 v = cross(w, u);
    return normalize(u * (std::cos(phi) * sin_t) + v * (std::sin(phi) * sin_t) + w * cos_t);
}

NE_INLINE Vec3 sample_disk(RNG& rng) {
    float r = std::sqrt(rng.next_float());
    float a = rng.next_float() * NE_TAU;
    return {r * std::cos(a), r * std::sin(a), 0.0f};
}

} // namespace ne

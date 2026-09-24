// ============================================================================
//  Native Engine — ne_scene.h
//  Materials, analytic primitives, meshes, camera, procedural sky, and the
//  scene with its intersection / light-sampling routines.
// ============================================================================
#pragma once

#include "ne_math.h"
#include "ne_bvh.h"
#include <vector>
#include <memory>
#include <atomic>

namespace ne {

// ------------------------------------------------------------ Material -----
struct Material {
    Vec3 albedo = Vec3(0.8f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    Vec3 emissive = Vec3(0.0f);
    float ior = 1.5f;
    float transmission = 0.0f; // 0 = opaque, 1 = glass
    float checker = 0.0f;      // 0 = off, otherwise checker tile size in world units
};

// ---------------------------------------------------------------- Hit ------
struct Hit {
    float t = 1e30f;
    Vec3 p, n;
    int mat = -1;
    int obj = -1;
};

// Transmissive-shadow tuning: how much light passes per glass crossing.
#define NE_GLASS_TINT 0.62f

// -------------------------------------------------------------- Camera -----
struct Camera {
    Vec3 pos = Vec3(0, 1, 4);
    Vec3 target = Vec3(0, 0, 0);
    float fov_y_deg = 55.0f;
    float aperture = 0.0f;   // lens radius (0 = pinhole)
    float focus_dist = 5.0f;

    void make_ray(float px, float py, int w, int h, RNG& rng, Vec3& o, Vec3& d) const {
        float aspect = (float)w / (float)h;
        float tan_half = std::tan(fov_y_deg * NE_PI / 360.0f);
        float sx = (2.0f * px / (float)w - 1.0f) * tan_half * aspect;
        float sy = (1.0f - 2.0f * py / (float)h) * tan_half;
        Vec3 fwd = normalize(target - pos);
        Vec3 right = normalize(cross(fwd, Vec3(0, 1, 0)));
        Vec3 up = cross(right, fwd);
        d = normalize(fwd + right * sx + up * sy);
        o = pos;
        if (aperture > 0.0f) {
            Vec3 lens = sample_disk(rng) * aperture;
            Vec3 focal = pos + d * (focus_dist / dot(d, fwd));
            o = pos + right * lens.x + up * lens.y;
            d = normalize(focal - o);
        }
    }
};

// ----------------------------------------------------------------- Sky -----
struct Sky {
    Vec3 top = Vec3(0.20f, 0.38f, 0.75f);
    Vec3 horizon = Vec3(0.78f, 0.72f, 0.66f);
    float intensity = 1.0f;
    Vec3 sun_dir = normalize(Vec3(0.45f, 0.65f, 0.3f));
    Vec3 sun_color = Vec3(1.0f, 0.93f, 0.82f);
    float sun_intensity = 4.5f;          // radiance of the disk
    float sun_cos_angle = std::cos(1.2f * NE_PI / 180.0f);

    Vec3 radiance(const Vec3& d) const {
        float t = clampf(d.y * 0.5f + 0.5f, 0.0f, 1.0f);
        Vec3 col = lerp(horizon, top, std::pow(t, 0.65f)) * intensity;
        float c = dot(d, sun_dir);
        if (c > sun_cos_angle) col += sun_color * (sun_intensity * 60.0f); // hot disk for reflections
        else col += sun_color * (0.12f * intensity) * std::pow(std::max(c, 0.0f), 6.0f);
        return col;
    }
};

// -------------------------------------------------------------- Object -----
enum class ObjType : uint8_t { Sphere, Box, Quad, Plane, Mesh };

struct Object {
    ObjType type;
    int mat = 0;
    int obj = -1;
    // analytic primitives
    Vec3 pos;            // sphere center / box center
    float radius = 1.0f; // sphere
    Vec3 half;           // box half extents
    float rot_y = 0.0f;  // box rotation around Y (degrees)
    Vec3 normal;         // plane normal
    float d = 0.0f;      // plane distance
    Vec3 q_p0, q_u, q_v; // quad: origin + edge vectors
    float q_area = 1.0f;
    Vec3 q_normal;
    // mesh
    std::shared_ptr<BVH> bvh;
    std::vector<float> local_verts; // local-space vertices (rotation applied at build)
    std::vector<uint32_t> indices;
    Vec3 pivot;          // rotation pivot for animated meshes

    AABB bounds() const {
        AABB b;
        switch (type) {
        case ObjType::Sphere: {
            Vec3 r(radius);
            b.grow(pos - r); b.grow(pos + r);
            break;
        }
        case ObjType::Box: {
            // Conservative bound of rotated box
            float a = rot_y * NE_PI / 180.0f;
            float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
            float ex = c * half.x + s * half.z;
            float ez = s * half.x + c * half.z;
            Vec3 r(ex, half.y, ez);
            b.grow(pos - r); b.grow(pos + r);
            break;
        }
        case ObjType::Quad: {
            b.grow(q_p0); b.grow(q_p0 + q_u); b.grow(q_p0 + q_v); b.grow(q_p0 + q_u + q_v);
            break;
        }
        case ObjType::Plane: {
            // Treat as a very large finite box for culling purposes
            b.grow(Vec3(-200.0f, d - 0.01f, -200.0f));
            b.grow(Vec3(200.0f, d + 0.01f, 200.0f));
            break;
        }
        case ObjType::Mesh: {
            if (bvh) for (const BVHTri& t : bvh->tris) {
                b.grow(t.v0); b.grow(t.v0 + t.e1); b.grow(t.v0 + t.e2);
            }
            break;
        }
        }
        return b;
    }
};

// ---------------------------------------------------------------- Scene ----
struct Scene {
    std::vector<Material> materials;
    std::vector<Object> objects;
    std::vector<int> lights;    // indices of objects with emissive material
    Sky sky;
    Camera camera;
    int max_bounces = 5;

    mutable std::atomic<uint64_t> ray_count{0};

    int add_material(const Material& m) {
        materials.push_back(m);
        return (int)materials.size() - 1;
    }

    void add_object(Object o) {
        o.obj = (int)objects.size();
        objects.push_back(o);
        if (o.mat >= 0 && o.mat < (int)materials.size() &&
            materials[o.mat].emissive.x + materials[o.mat].emissive.y + materials[o.mat].emissive.z > 0.0f)
            lights.push_back(o.obj);
    }

    // Build / rebuild a mesh object's BVH from its local verts + transform.
    void rebuild_mesh(Object& o) {
        size_t vcount = o.local_verts.size() / 3;
        std::vector<float> transformed(vcount * 3);
        Mat4 rot = Mat4::rot_y(o.rot_y);
        for (size_t i = 0; i < vcount; ++i) {
            Vec3 v(o.local_verts[i * 3], o.local_verts[i * 3 + 1], o.local_verts[i * 3 + 2]);
            Vec3 t = rot.transform_point(v) + o.pos;
            transformed[i * 3] = t.x; transformed[i * 3 + 1] = t.y; transformed[i * 3 + 2] = t.z;
        }
        o.bvh = std::make_shared<BVH>();
        o.bvh->build(transformed.data(), o.indices.data(), (uint32_t)o.indices.size() / 3);
    }

    void finalize() {
        for (Object& o : objects)
            if (o.type == ObjType::Mesh) rebuild_mesh(o);
    }

    // ---------------------------------------------------------- intersect ---
    bool intersect(const Vec3& o, const Vec3& d, Hit& hit) const {
        ray_count.fetch_add(1, std::memory_order_relaxed);
        float best = hit.t;
        bool found = false;

        for (const Object& ob : objects) {
            switch (ob.type) {

            case ObjType::Sphere: {
                Vec3 oc = o - ob.pos;
                float b = dot(oc, d);
                float c = dot(oc, oc) - ob.radius * ob.radius;
                float disc = b * b - c;
                if (disc < 0) break;
                float sq = std::sqrt(disc);
                float t = -b - sq;
                if (t < 1e-3f) t = -b + sq;
                if (t > 1e-3f && t < best) {
                    best = t;
                    hit.p = o + d * t;
                    hit.n = (hit.p - ob.pos) / ob.radius;
                    hit.mat = ob.mat;
                    hit.obj = ob.obj;
                    found = true;
                }
                break;
            }

            case ObjType::Box: {
                float a = -ob.rot_y * NE_PI / 180.0f;
                float ca = std::cos(a), sa = std::sin(a);
                Vec3 lo = o - ob.pos;
                Vec3 ol(lo.x * ca - lo.z * sa, lo.y, lo.x * sa + lo.z * ca);
                Vec3 dl(d.x * ca - d.z * sa, d.y, d.x * sa + d.z * ca);
                Vec3 inv = 1.0f / dl;
                float t0 = (ob.half.x - ol.x) * inv.x, t1 = (-ob.half.x - ol.x) * inv.x;
                float tmin = std::min(t0, t1), tmaxb = std::max(t0, t1);
                int axis = 0; float sign = t0 < t1 ? 1.0f : -1.0f;
                t0 = (ob.half.y - ol.y) * inv.y; t1 = (-ob.half.y - ol.y) * inv.y;
                if (std::min(t0, t1) > tmin) { tmin = std::min(t0, t1); axis = 1; sign = t0 < t1 ? 1.0f : -1.0f; }
                tmaxb = std::min(tmaxb, std::max(t0, t1));
                t0 = (ob.half.z - ol.z) * inv.z; t1 = (-ob.half.z - ol.z) * inv.z;
                if (std::min(t0, t1) > tmin) { tmin = std::min(t0, t1); axis = 2; sign = t0 < t1 ? 1.0f : -1.0f; }
                tmaxb = std::min(tmaxb, std::max(t0, t1));
                if (tmaxb < std::max(tmin, 0.0f) || tmin > best || tmin < 1e-3f) break;
                best = tmin;
                Vec3 lp = ol + dl * tmin;
                Vec3 ln(0.0f);
                if (axis == 0) { ln.x = lp.x >= 0.0f ? 1.0f : -1.0f; }
                else if (axis == 1) { ln.y = lp.y >= 0.0f ? 1.0f : -1.0f; }
                else { ln.z = lp.z >= 0.0f ? 1.0f : -1.0f; }
                (void)sign;
                // rotate normal back to world
                float a2 = ob.rot_y * NE_PI / 180.0f;
                float c2 = std::cos(a2), s2 = std::sin(a2);
                hit.n = Vec3(ln.x * c2 - ln.z * s2, ln.y, ln.x * s2 + ln.z * c2);
                hit.p = o + d * tmin;
                hit.mat = ob.mat;
                hit.obj = ob.obj;
                found = true;
                break;
            }

            case ObjType::Plane: {
                float denom = dot(ob.normal, d);
                if (std::fabs(denom) < 1e-8f) break;
                float t = (ob.d - dot(ob.normal, o)) / denom;
                if (t > 1e-3f && t < best) {
                    best = t;
                    hit.p = o + d * t;
                    hit.n = dot(ob.normal, d) < 0.0f ? ob.normal : -ob.normal;
                    hit.mat = ob.mat;
                    hit.obj = ob.obj;
                    found = true;
                }
                break;
            }

            case ObjType::Quad: {
                float denom = dot(ob.q_normal, d);
                if (std::fabs(denom) < 1e-8f) break;
                float t = dot(ob.q_p0 - o, ob.q_normal) / denom;
                if (t <= 1e-3f || t >= best) break;
                Vec3 p = o + d * t;
                Vec3 rel = p - ob.q_p0;
                float uu = dot(rel, ob.q_u) / dot(ob.q_u, ob.q_u);
                float vv = dot(rel, ob.q_v) / dot(ob.q_v, ob.q_v);
                if (uu < 0.0f || uu > 1.0f || vv < 0.0f || vv > 1.0f) break;
                best = t;
                hit.p = p;
                hit.n = denom < 0.0f ? ob.q_normal : -ob.q_normal;
                hit.mat = ob.mat;
                hit.obj = ob.obj;
                found = true;
                break;
            }

            case ObjType::Mesh: {
                float t = best;
                Vec3 n;
                if (ob.bvh && ob.bvh->intersect(o, d, t, n)) {
                    best = t;
                    hit.n = n;
                    hit.p = o + d * t;
                    hit.mat = ob.mat;
                    hit.obj = ob.obj;
                    found = true;
                }
                break;
            }
            }
        }
        if (found) { hit.t = best; }
        return found;
    }

    bool occluded(const Vec3& o, const Vec3& d, float tmax) const {
        ray_count.fetch_add(1, std::memory_order_relaxed);
        Hit tmp;
        tmp.t = tmax;
        for (const Object& ob : objects) {
            switch (ob.type) {
            case ObjType::Sphere: {
                Vec3 oc = o - ob.pos;
                float b = dot(oc, d);
                float c = dot(oc, oc) - ob.radius * ob.radius;
                float disc = b * b - c;
                if (disc < 0) break;
                float sq = std::sqrt(disc);
                float t = -b - sq;
                if (t < 1e-3f) t = -b + sq;
                if (t > 1e-3f && t < tmax) return true;
                break;
            }
            case ObjType::Box: {
                float a = -ob.rot_y * NE_PI / 180.0f;
                float ca = std::cos(a), sa = std::sin(a);
                Vec3 lo = o - ob.pos;
                Vec3 ol(lo.x * ca - lo.z * sa, lo.y, lo.x * sa + lo.z * ca);
                Vec3 dl(d.x * ca - d.z * sa, d.y, d.x * sa + d.z * ca);
                Vec3 inv = 1.0f / dl;
                float t0 = (ob.half.x - ol.x) * inv.x, t1 = (-ob.half.x - ol.x) * inv.x;
                float tmin = std::min(t0, t1), tmaxb = std::max(t0, t1);
                t0 = (ob.half.y - ol.y) * inv.y; t1 = (-ob.half.y - ol.y) * inv.y;
                tmin = std::max(tmin, std::min(t0, t1)); tmaxb = std::min(tmaxb, std::max(t0, t1));
                t0 = (ob.half.z - ol.z) * inv.z; t1 = (-ob.half.z - ol.z) * inv.z;
                tmin = std::max(tmin, std::min(t0, t1)); tmaxb = std::min(tmaxb, std::max(t0, t1));
                if (tmaxb >= std::max(tmin, 0.0f) && tmin < tmax && tmin > 1e-3f) return true;
                break;
            }
            case ObjType::Plane: {
                float denom = dot(ob.normal, d);
                if (std::fabs(denom) < 1e-8f) break;
                float t = (ob.d - dot(ob.normal, o)) / denom;
                if (t > 1e-3f && t < tmax) return true;
                break;
            }
            case ObjType::Quad: {
                float denom = dot(ob.q_normal, d);
                if (std::fabs(denom) < 1e-8f) break;
                float t = dot(ob.q_p0 - o, ob.q_normal) / denom;
                if (t <= 1e-3f || t >= tmax) break;
                Vec3 p = o + d * t;
                Vec3 rel = p - ob.q_p0;
                float uu = dot(rel, ob.q_u) / dot(ob.q_u, ob.q_u);
                float vv = dot(rel, ob.q_v) / dot(ob.q_v, ob.q_v);
                if (uu >= 0.0f && uu <= 1.0f && vv >= 0.0f && vv <= 1.0f) return true;
                break;
            }
            case ObjType::Mesh: {
                if (ob.bvh && ob.bvh->occluded(o, d, tmax)) return true;
                break;
            }
            }
        }
        (void)tmp;
        return false;
    }

    // Sample a point on light object `obj`. Returns Le and fills point/normal.

    // ------------------------------------------------------- transmission ---
    // Shadow transmittance: 0 = fully blocked, 1 = clear.
    // Transmissive objects (glass) tint the light instead of blocking it.
    float shadow_transmittance(const Vec3& o, const Vec3& d, float tmax) const {
        float T = 1.0f;
        Vec3 oo = o;
        float remaining = tmax;
        for (int iter = 0; iter < 4; ++iter) {
            Hit h;
            h.t = remaining;
            if (!intersect(oo, d, h)) return T;
            const Material& m = materials[h.mat];
            if (m.transmission <= 0.0f) return 0.0f;
            T *= NE_GLASS_TINT;
            // advance past the transmissive object
            float push = 0.02f;
            const Object& ob = objects[h.obj];
            if (ob.type == ObjType::Sphere) {
                Vec3 oc = oo - ob.pos;
                float b = dot(oc, d);
                float disc = b * b - (dot(oc, oc) - ob.radius * ob.radius);
                push = -b + std::sqrt(std::max(disc, 0.0f)) + 1e-3f;
            } else if (ob.type == ObjType::Box) {
                push = 2.0f * std::max(ob.half.x, std::max(ob.half.y, ob.half.z)) + 1e-3f;
            }
            oo = h.p + d * push;
            remaining -= push;
            if (remaining <= 0.0f) break;
        }
        return T;
    }

    Vec3 sample_light(int obj, RNG& rng, Vec3& point, Vec3& normal, float& area) const {
        const Object& o = objects[obj];
        switch (o.type) {
        case ObjType::Sphere: {
            normal = normalize(random_in_unit_sphere(rng));
            point = o.pos + normal * (o.radius * 1.0001f);
            area = 4.0f * NE_PI * o.radius * o.radius;
            break;
        }
        case ObjType::Quad: {
            float r1 = rng.next_float(), r2 = rng.next_float();
            point = o.q_p0 + o.q_u * r1 + o.q_v * r2;
            normal = o.q_normal;
            area = o.q_area;
            break;
        }
        case ObjType::Box: {
            float hx = o.half.x, hy = o.half.y, hz = o.half.z;
            float ax = hy * hz, ay = hx * hz, az = hx * hy;
            float total = 2.0f * (ax + ay + az);
            float r = rng.next_float() * total;
            float u = rng.next_float() * 2.0f - 1.0f, v = rng.next_float() * 2.0f - 1.0f;
            Vec3 lp(0.0f), ln(0.0f);
            if (r < 2 * ax) { lp = Vec3(hx, u * hy, v * hz); ln = Vec3(1, 0, 0); if (rng.next_float() < 0.5f) { lp.x = -hx; ln = Vec3(-1,0,0); } }
            else if (r < 2 * (ax + ay)) { lp = Vec3(u * hx, hy, v * hz); ln = Vec3(0, 1, 0); if (rng.next_float() < 0.5f) { lp.y = -hy; ln = Vec3(0,-1,0); } }
            else { lp = Vec3(u * hx, v * hy, hz); ln = Vec3(0, 0, 1); if (rng.next_float() < 0.5f) { lp.z = -hz; ln = Vec3(0,0,-1); } }
            float a = o.rot_y * NE_PI / 180.0f;
            float ca = std::cos(a), sa = std::sin(a);
            point = o.pos + Vec3(lp.x * ca - lp.z * sa, lp.y, lp.x * sa + lp.z * ca);
            normal = Vec3(ln.x * ca - ln.z * sa, ln.y, ln.x * sa + ln.z * ca);
            area = total;
            break;
        }
        default:
            point = o.pos; normal = Vec3(0, 1, 0); area = 1.0f;
            break;
        }
        return materials[o.mat].emissive;
    }
};

} // namespace ne

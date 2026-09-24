// ============================================================================
//  Native Engine — ne_bvh.h
//  Bounding Volume Hierarchy over triangle meshes (median split, header-only).
// ============================================================================
#pragma once

#include "ne_math.h"
#include <vector>

namespace ne {

struct BVHTri { Vec3 v0, e1, e2, n; }; // precomputed edges + geometric normal

struct BVHNode {
    AABB bounds;
    uint32_t left_first = 0; // interior: left child index, leaf: offset into perm
    uint32_t right = 0;      // interior: right child index (unused for leaves)
    uint32_t count = 0;      // 0 => interior, >0 => leaf triangle count
};

struct BVH {
    std::vector<BVHTri> tris;
    std::vector<BVHNode> nodes;
    std::vector<uint32_t> perm; // leaf triangle index order
    uint32_t nodes_used = 0;

    void build(const float* verts, const uint32_t* indices, uint32_t tri_count) {
        tris.clear();
        perm.clear();
        tris.reserve(tri_count);
        perm.reserve(tri_count);
        for (uint32_t i = 0; i < tri_count; ++i) {
            Vec3 a(verts[indices[i * 3] * 3],     verts[indices[i * 3] * 3 + 1],     verts[indices[i * 3] * 3 + 2]);
            Vec3 b(verts[indices[i * 3 + 1] * 3], verts[indices[i * 3 + 1] * 3 + 1], verts[indices[i * 3 + 1] * 3 + 2]);
            Vec3 c(verts[indices[i * 3 + 2] * 3], verts[indices[i * 3 + 2] * 3 + 1], verts[indices[i * 3 + 2] * 3 + 2]);
            BVHTri t;
            t.v0 = a;
            t.e1 = b - a;
            t.e2 = c - a;
            t.n = normalize(cross(t.e1, t.e2));
            tris.push_back(t);
        }
        std::vector<uint32_t> idx(tri_count);
        for (uint32_t i = 0; i < tri_count; ++i) idx[i] = i;
        nodes.clear();
        nodes.resize(std::max(2u, 2 * tri_count + 1));
        nodes_used = 0;
        build_recursive(idx.data(), tri_count);
        nodes.resize(nodes_used);
    }

    uint32_t build_recursive(uint32_t* idx, uint32_t count) {
        uint32_t node = nodes_used++;
        AABB bounds;
        Vec3 cmin(1e30f), cmax(-1e30f);
        for (uint32_t i = 0; i < count; ++i) {
            const BVHTri& t = tris[idx[i]];
            bounds.grow(t.v0);
            bounds.grow(t.v0 + t.e1);
            bounds.grow(t.v0 + t.e2);
            Vec3 c = (t.v0 * 2.0f + t.e1 + t.e2) / 3.0f;
            cmin = {std::min(cmin.x, c.x), std::min(cmin.y, c.y), std::min(cmin.z, c.z)};
            cmax = {std::max(cmax.x, c.x), std::max(cmax.y, c.y), std::max(cmax.z, c.z)};
        }
        nodes[node].bounds = bounds;

        if (count <= 8) {
            uint32_t off = (uint32_t)perm.size();
            perm.insert(perm.end(), idx, idx + count);
            nodes[node].left_first = off;
            nodes[node].count = count;
            return node;
        }

        Vec3 ext = cmax - cmin;
        int axis = ext.x > ext.y ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);
        float split = (axis == 0 ? cmin.x : axis == 1 ? cmin.y : cmin.z) +
                      (axis == 0 ? ext.x : axis == 1 ? ext.y : ext.z) * 0.5f;
        uint32_t i = 0, j = count;
        while (i < j) {
            const BVHTri& t = tris[idx[i]];
            Vec3 c = (t.v0 * 2.0f + t.e1 + t.e2) / 3.0f;
            float v = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
            if (v < split) ++i;
            else std::swap(idx[i], idx[--j]);
        }
        if (i == 0 || i == count) i = count / 2; // degenerate fallback

        // Recurse first; record actual child indices (subtrees are not contiguous).
        uint32_t left = build_recursive(idx, i);
        uint32_t right = build_recursive(idx + i, count - i);
        nodes[node].left_first = left;
        nodes[node].right = right;
        nodes[node].count = 0;
        return node;
    }

    // Moller-Trumbore. Returns t or -1.
    static NE_INLINE float ray_tri(const BVHTri& t, const Vec3& o, const Vec3& d) {
        Vec3 pvec = cross(d, t.e2);
        float det = dot(t.e1, pvec);
        if (std::fabs(det) < 1e-9f) return -1.0f;
        float inv_det = 1.0f / det;
        Vec3 tvec = o - t.v0;
        float u = dot(tvec, pvec) * inv_det;
        if (u < 0.0f || u > 1.0f) return -1.0f;
        Vec3 qvec = cross(tvec, t.e1);
        float v = dot(d, qvec) * inv_det;
        if (v < 0.0f || u + v > 1.0f) return -1.0f;
        return dot(t.e2, qvec) * inv_det;
    }

    // Nearest hit. `t` is in/out current best distance; on hit `t` and `n` updated.
    NE_INLINE bool intersect(const Vec3& o, const Vec3& d, float& t, Vec3& n) const {
        Vec3 inv_d = 1.0f / d;
        bool hit = false;
        uint32_t stack[64];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0) {
            const BVHNode& node = nodes[stack[--sp]];
            if (node.count > 0) { // leaf
                for (uint32_t i = 0; i < node.count; ++i) {
                    const BVHTri& tri = tris[perm[node.left_first + i]];
                    float th = ray_tri(tri, o, d);
                    if (th > 1e-3f && th < t) {
                        t = th;
                        n = tri.n;
                        hit = true;
                    }
                }
            } else {
                uint32_t l = node.left_first, r = node.right;
                float tl = nodes[l].bounds.intersect(o, inv_d, t);
                float tr = nodes[r].bounds.intersect(o, inv_d, t);
                if (tl >= 0 && tr >= 0) {
                    if (tl < tr) { stack[sp++] = r; stack[sp++] = l; }
                    else         { stack[sp++] = l; stack[sp++] = r; }
                } else if (tl >= 0) stack[sp++] = l;
                else if (tr >= 0)   stack[sp++] = r;
            }
        }
        return hit;
    }

    // Any hit (shadow rays). Returns true if a triangle blocks within tmax.
    NE_INLINE bool occluded(const Vec3& o, const Vec3& d, float tmax) const {
        Vec3 inv_d = 1.0f / d;
        uint32_t stack[64];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0) {
            const BVHNode& node = nodes[stack[--sp]];
            if (node.count > 0) {
                for (uint32_t i = 0; i < node.count; ++i) {
                    const BVHTri& tri = tris[perm[node.left_first + i]];
                    float th = ray_tri(tri, o, d);
                    if (th > 1e-3f && th < tmax) return true;
                }
            } else {
                uint32_t l = node.left_first, r = node.right;
                float tl = nodes[l].bounds.intersect(o, inv_d, tmax);
                float tr = nodes[r].bounds.intersect(o, inv_d, tmax);
                if (tl >= 0) stack[sp++] = l;
                if (tr >= 0 && sp < 62) stack[sp++] = r;
            }
        }
        return false;
    }
};

} // namespace ne

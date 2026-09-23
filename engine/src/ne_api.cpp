// ============================================================================
//  Native Engine — ne_api.cpp
//  Implementation of the C ABI on top of the renderer core.
// ============================================================================
#include "ne_api.h"
#include "ne_renderer.h"
#include <cstring>
#include <new>

using namespace ne;

struct NEEngine_Shim {
    Renderer renderer;
    std::vector<uint8_t> png_out;
    explicit NEEngine_Shim(int w, int h) : renderer(w, h) {}
};

#define SHIM reinterpret_cast<NEEngine_Shim*>(e)

const char* ne_version() { return "Native Engine 1.0.0 (ray-traced)"; }

NEEngine* ne_create(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    return reinterpret_cast<NEEngine*>(new (std::nothrow) NEEngine_Shim(width, height));
}

void ne_destroy(NEEngine* e) { delete SHIM; }

void ne_resize(NEEngine* e, int width, int height) {
    if (SHIM && width > 0 && height > 0) SHIM->renderer.resize(width, height);
}

int ne_add_material(NEEngine* e, const float albedo[3], float metallic, float roughness,
                    const float emissive[3], float ior, float transmission, float checker) {
    if (!SHIM) return -1;
    Material m;
    m.albedo = Vec3(albedo[0], albedo[1], albedo[2]);
    m.metallic = metallic;
    m.roughness = roughness;
    m.emissive = Vec3(emissive[0], emissive[1], emissive[2]);
    m.ior = ior;
    m.transmission = transmission;
    m.checker = checker;
    return SHIM->renderer.scene.add_material(m);
}

int ne_update_material(NEEngine* e, int idx, const float albedo[3], float metallic,
                       float roughness, const float emissive[3]) {
    if (!SHIM || idx < 0 || idx >= (int)SHIM->renderer.scene.materials.size()) return -1;
    Material& m = SHIM->renderer.scene.materials[idx];
    m.albedo = Vec3(albedo[0], albedo[1], albedo[2]);
    m.metallic = metallic;
    m.roughness = roughness;
    m.emissive = Vec3(emissive[0], emissive[1], emissive[2]);
    return 0;
}

int ne_add_sphere(NEEngine* e, int mat, const float pos[3], float radius) {
    if (!SHIM) return -1;
    Object o;
    o.type = ObjType::Sphere;
    o.mat = mat;
    o.pos = Vec3(pos[0], pos[1], pos[2]);
    o.radius = radius;
    SHIM->renderer.scene.add_object(o);
    return o.obj;
}

int ne_add_box(NEEngine* e, int mat, const float pos[3], const float half[3], float rot_y_deg) {
    if (!SHIM) return -1;
    Object o;
    o.type = ObjType::Box;
    o.mat = mat;
    o.pos = Vec3(pos[0], pos[1], pos[2]);
    o.half = Vec3(half[0], half[1], half[2]);
    o.rot_y = rot_y_deg;
    SHIM->renderer.scene.add_object(o);
    return o.obj;
}

int ne_add_quad(NEEngine* e, int mat, const float p0[3], const float u[3], const float v[3]) {
    if (!SHIM) return -1;
    Object o;
    o.type = ObjType::Quad;
    o.mat = mat;
    o.q_p0 = Vec3(p0[0], p0[1], p0[2]);
    o.q_u = Vec3(u[0], u[1], u[2]);
    o.q_v = Vec3(v[0], v[1], v[2]);
    o.q_normal = normalize(cross(o.q_u, o.q_v));
    o.q_area = length(cross(o.q_u, o.q_v));
    SHIM->renderer.scene.add_object(o);
    return o.obj;
}

int ne_add_plane(NEEngine* e, int mat, float y) {
    if (!SHIM) return -1;
    Object o;
    o.type = ObjType::Plane;
    o.mat = mat;
    o.normal = Vec3(0, 1, 0);
    o.d = y;
    SHIM->renderer.scene.add_object(o);
    return o.obj;
}

int ne_add_mesh(NEEngine* e, int mat, const float* verts, int vert_count,
                const unsigned* indices, int index_count) {
    if (!SHIM || !verts || !indices || vert_count <= 0 || index_count <= 0) return -1;
    Object o;
    o.type = ObjType::Mesh;
    o.mat = mat;
    o.local_verts.assign(verts, verts + (size_t)vert_count * 3);
    o.indices.assign(indices, indices + (size_t)index_count);
    SHIM->renderer.scene.add_object(o);
    // Build immediately; later transform changes rebuild lazily.
    SHIM->renderer.scene.rebuild_mesh(SHIM->renderer.scene.objects.back());
    return o.obj;
}

void ne_set_object_transform(NEEngine* e, int obj, const float pos[3], float rot_y_deg) {
    if (!SHIM || obj < 0 || obj >= (int)SHIM->renderer.scene.objects.size()) return;
    Object& o = SHIM->renderer.scene.objects[obj];
    o.pos = Vec3(pos[0], pos[1], pos[2]);
    o.rot_y = rot_y_deg;
    if (o.type == ObjType::Mesh) SHIM->renderer.scene.rebuild_mesh(o);
}

void ne_set_camera(NEEngine* e, const float pos[3], const float target[3], float fov_y_deg,
                   float aperture, float focus_dist) {
    if (!SHIM) return;
    Camera& c = SHIM->renderer.scene.camera;
    c.pos = Vec3(pos[0], pos[1], pos[2]);
    c.target = Vec3(target[0], target[1], target[2]);
    c.fov_y_deg = fov_y_deg;
    c.aperture = aperture;
    c.focus_dist = focus_dist;
}

void ne_set_sky(NEEngine* e, const float top[3], const float horizon[3],
                const float sun_dir[3], const float sun_color[3], float sun_intensity,
                float sun_radius_deg, float sky_intensity) {
    if (!SHIM) return;
    Sky& s = SHIM->renderer.scene.sky;
    s.top = normalize(Vec3(top[0], top[1], top[2]));
    s.top = Vec3(top[0], top[1], top[2]);
    s.horizon = Vec3(horizon[0], horizon[1], horizon[2]);
    s.sun_dir = normalize(Vec3(sun_dir[0], sun_dir[1], sun_dir[2]));
    s.sun_color = Vec3(sun_color[0], sun_color[1], sun_color[2]);
    s.sun_intensity = sun_intensity;
    s.sun_cos_angle = std::cos(std::max(0.05f, sun_radius_deg) * NE_PI / 180.0f);
    s.intensity = sky_intensity;
}

void ne_set_max_bounces(NEEngine* e, int bounces) {
    if (SHIM && bounces > 0 && bounces < 64) SHIM->renderer.scene.max_bounces = bounces;
}

void ne_set_streaming(NEEngine* e, int on) { if (SHIM) SHIM->renderer.streaming = on != 0; }
void ne_reset_accum(NEEngine* e) { if (SHIM) SHIM->renderer.reset_accum(); }

int ne_render(NEEngine* e, int spp) {
    if (!SHIM || spp < 1) return -1;
    SHIM->renderer.render(spp);
    return 0;
}

const unsigned char* ne_frame_rgba(NEEngine* e) {
    if (!SHIM) return nullptr;
    SHIM->renderer.get_rgba(SHIM->png_out);
    return SHIM->png_out.data();
}

const unsigned char* ne_frame_png(NEEngine* e, int* out_len) {
    if (!SHIM) { if (out_len) *out_len = 0; return nullptr; }
    std::vector<uint8_t> png;
    SHIM->renderer.get_png(png);
    SHIM->png_out = std::move(png);
    if (out_len) *out_len = (int)SHIM->png_out.size();
    return SHIM->png_out.data();
}

double ne_stat_ms(NEEngine* e) { return SHIM ? SHIM->renderer.stats.last_ms : 0.0; }
double ne_stat_mrays(NEEngine* e) { return SHIM ? SHIM->renderer.stats.last_mrays : 0.0; }
int ne_stat_accum(NEEngine* e) { return SHIM ? SHIM->renderer.stats.accum_samples : 0; }
int ne_stat_objects(NEEngine* e) {
    return SHIM ? (int)SHIM->renderer.scene.objects.size() : 0;
}

int ne_stat_threads(NEEngine* e) {
    (void)e;
    return (int)std::max(1u, std::thread::hardware_concurrency());
}

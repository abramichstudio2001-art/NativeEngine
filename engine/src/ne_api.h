// ============================================================================
//  Native Engine — ne_api.h
//  Stable C ABI exported by libNativeEngine — consumed from Python via ctypes.
// ============================================================================
#pragma once

#include <cstdint>

#ifdef _WIN32
#define NE_EXPORT extern "C" __declspec(dllexport)
#else
#define NE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

typedef struct NEEngine NEEngine;

NE_EXPORT const char* ne_version();
NE_EXPORT NEEngine*   ne_create(int width, int height);
NE_EXPORT void        ne_destroy(NEEngine* e);
NE_EXPORT void        ne_resize(NEEngine* e, int width, int height);

// ---- materials ------------------------------------------------------------
NE_EXPORT int  ne_add_material(NEEngine* e, const float albedo[3],
                               float metallic, float roughness,
                               const float emissive[3],
                               float ior, float transmission, float checker);
NE_EXPORT int  ne_update_material(NEEngine* e, int idx, const float albedo[3],
                                  float metallic, float roughness,
                                  const float emissive[3]);

// ---- geometry ---------------------------------------------------------------
NE_EXPORT int  ne_add_sphere(NEEngine* e, int mat, const float pos[3], float radius);
NE_EXPORT int  ne_add_box(NEEngine* e, int mat, const float pos[3],
                          const float half[3], float rot_y_deg);
NE_EXPORT int  ne_add_quad(NEEngine* e, int mat, const float p0[3],
                           const float u[3], const float v[3]);
NE_EXPORT int  ne_add_plane(NEEngine* e, int mat, float y);
NE_EXPORT int  ne_add_mesh(NEEngine* e, int mat, const float* verts, int vert_count,
                           const unsigned* indices, int index_count);
NE_EXPORT void ne_set_object_transform(NEEngine* e, int obj, const float pos[3],
                                       float rot_y_deg);
NE_EXPORT int  ne_stat_objects(NEEngine* e);

// ---- environment / camera ---------------------------------------------------
NE_EXPORT void ne_set_camera(NEEngine* e, const float pos[3], const float target[3],
                             float fov_y_deg, float aperture, float focus_dist);
NE_EXPORT void ne_set_sky(NEEngine* e, const float top[3], const float horizon[3],
                          const float sun_dir[3], const float sun_color[3],
                          float sun_intensity, float sun_radius_deg, float sky_intensity);
NE_EXPORT void ne_set_max_bounces(NEEngine* e, int bounces);

// ---- rendering ---------------------------------------------------------------
NE_EXPORT void ne_set_streaming(NEEngine* e, int on);
NE_EXPORT void ne_reset_accum(NEEngine* e);
NE_EXPORT int  ne_render(NEEngine* e, int spp);            // returns 0 on success
NE_EXPORT const unsigned char* ne_frame_rgba(NEEngine* e);
NE_EXPORT const unsigned char* ne_frame_png(NEEngine* e, int* out_len);

// ---- stats --------------------------------------------------------------------
NE_EXPORT double ne_stat_ms(NEEngine* e);
NE_EXPORT double ne_stat_mrays(NEEngine* e);
NE_EXPORT int    ne_stat_accum(NEEngine* e);
NE_EXPORT int    ne_stat_threads(NEEngine* e);

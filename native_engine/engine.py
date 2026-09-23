# ============================================================================
#  Native Engine — engine.py
#  Pythonic bindings + game-engine API on top of the C++ ray-tracing core.
#
#  Quick start:
#      import native_engine as ne
#      app = ne.App(width=1280, height=720)
#      gold = app.metal((1.0, 0.71, 0.29), roughness=0.15)
#      app.sphere(gold, pos=(-1.6, 0.8, 0), radius=0.8)
#      app.ground(checker=1.5)
#      app.sun(direction=(0.4, 0.75, 0.3))
#      app.save("shot.png", samples=128)
# ============================================================================
import ctypes
import math
import os
import sys
import threading

from . import build as _build

# ------------------------------------------------------------------- types --
class _CEngine(ctypes.Structure):
    pass


def _load():
    lib_path = _build.build()
    lib = ctypes.CDLL(lib_path)
    P = ctypes.POINTER

    lib.ne_version.restype = ctypes.c_char_p
    lib.ne_create.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.ne_create.restype = P(_CEngine)
    lib.ne_destroy.argtypes = [P(_CEngine)]
    lib.ne_resize.argtypes = [P(_CEngine), ctypes.c_int, ctypes.c_int]

    f3 = ctypes.POINTER(ctypes.c_float)

    lib.ne_add_material.argtypes = [P(_CEngine), f3, ctypes.c_float, ctypes.c_float,
                                    f3, ctypes.c_float, ctypes.c_float, ctypes.c_float]
    lib.ne_add_material.restype = ctypes.c_int
    lib.ne_update_material.argtypes = [P(_CEngine), ctypes.c_int, f3,
                                       ctypes.c_float, ctypes.c_float, f3]
    lib.ne_add_sphere.argtypes = [P(_CEngine), ctypes.c_int, f3, ctypes.c_float]
    lib.ne_add_sphere.restype = ctypes.c_int
    lib.ne_add_box.argtypes = [P(_CEngine), ctypes.c_int, f3, f3, ctypes.c_float]
    lib.ne_add_box.restype = ctypes.c_int
    lib.ne_add_quad.argtypes = [P(_CEngine), ctypes.c_int, f3, f3, f3]
    lib.ne_add_quad.restype = ctypes.c_int
    lib.ne_add_plane.argtypes = [P(_CEngine), ctypes.c_int, ctypes.c_float]
    lib.ne_add_plane.restype = ctypes.c_int
    lib.ne_add_mesh.argtypes = [P(_CEngine), ctypes.c_int, f3, ctypes.c_int,
                                ctypes.POINTER(ctypes.c_uint), ctypes.c_int]
    lib.ne_add_mesh.restype = ctypes.c_int
    lib.ne_set_object_transform.argtypes = [P(_CEngine), ctypes.c_int, f3, ctypes.c_float]
    lib.ne_stat_objects.argtypes = [P(_CEngine)]
    lib.ne_stat_objects.restype = ctypes.c_int
    lib.ne_set_camera.argtypes = [P(_CEngine), f3, f3, ctypes.c_float,
                                  ctypes.c_float, ctypes.c_float]
    lib.ne_set_sky.argtypes = [P(_CEngine), f3, f3, f3, f3,
                               ctypes.c_float, ctypes.c_float, ctypes.c_float]
    lib.ne_set_max_bounces.argtypes = [P(_CEngine), ctypes.c_int]
    lib.ne_set_streaming.argtypes = [P(_CEngine), ctypes.c_int]
    lib.ne_reset_accum.argtypes = [P(_CEngine)]
    lib.ne_render.argtypes = [P(_CEngine), ctypes.c_int]
    lib.ne_render.restype = ctypes.c_int
    lib.ne_frame_png.argtypes = [P(_CEngine), P(ctypes.c_int)]
    lib.ne_frame_png.restype = ctypes.POINTER(ctypes.c_ubyte)
    lib.ne_frame_rgba.argtypes = [P(_CEngine)]
    lib.ne_frame_rgba.restype = ctypes.POINTER(ctypes.c_ubyte)
    lib.ne_stat_ms.argtypes = [P(_CEngine)]
    lib.ne_stat_ms.restype = ctypes.c_double
    lib.ne_stat_mrays.argtypes = [P(_CEngine)]
    lib.ne_stat_mrays.restype = ctypes.c_double
    lib.ne_stat_accum.argtypes = [P(_CEngine)]
    lib.ne_stat_accum.restype = ctypes.c_int
    lib.ne_stat_threads.argtypes = [P(_CEngine)]
    lib.ne_stat_threads.restype = ctypes.c_int
    return lib


class Vec3:
    """Tiny 3-component helper (also accepts plain tuples/lists)."""
    __slots__ = ("x", "y", "z")

    def __init__(self, x=0.0, y=0.0, z=0.0):
        if isinstance(x, Vec3):
            self.x, self.y, self.z = x.x, x.y, x.z
        elif isinstance(x, (tuple, list)):
            self.x, self.y, self.z = (float(x[0]), float(x[1]), float(x[2]))
        else:
            self.x, self.y, self.z = float(x), float(y), float(z)

    def __add__(self, o): return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)
    def __sub__(self, o): return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)
    def __mul__(self, s): return Vec3(self.x * s, self.y * s, self.z * s)
    __rmul__ = __mul__

    def normalized(self):
        import math
        l = math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z) or 1.0
        return Vec3(self.x / l, self.y / l, self.z / l)

    def tuple(self):
        return (self.x, self.y, self.z)

    def __repr__(self):
        return f"Vec3({self.x:.3f}, {self.y:.3f}, {self.z:.3f})"


def _f3(v):
    """Convert to a ctypes float[3]."""
    if isinstance(v, (Vec3, tuple, list)):
        v = Vec3(v)
        return (ctypes.c_float * 3)(v.x, v.y, v.z)
    return (ctypes.c_float * 3)(float(v), float(v), float(v))


class Material:
    """Handle to a material inside the engine's material table."""
    def __init__(self, engine, index):
        self._engine = engine
        self.index = index
        engine._materials.append(self)

    def update(self, albedo=None, metallic=None, roughness=None, emissive=None):
        """Live-update material properties (takes effect on the next frame)."""
        e = self._engine
        m = e._mat_cache[self.index]
        if albedo is not None:
            m["albedo"] = Vec3(albedo).tuple()
        if metallic is not None:
            m["metallic"] = float(metallic)
        if roughness is not None:
            m["roughness"] = max(1e-3, float(roughness))
        if emissive is not None:
            m["emissive"] = Vec3(emissive).tuple()
        a = _f3(m["albedo"]); em = _f3(m["emissive"])
        e._lib.ne_update_material(e._ptr, self.index, a, m["metallic"],
                                  m["roughness"], em)
        return self


class Object:
    """Handle to a scene object."""
    def __init__(self, engine, index, kind):
        self._engine = engine
        self.index = index
        self.kind = kind

    def set_transform(self, pos=None, rot_y=0.0):
        e = self._engine
        p = Vec3(pos) if pos is not None else Vec3(0, 0, 0)
        e._lib.ne_set_object_transform(e._ptr, self.index, _f3(p), float(rot_y))
        return self


class App:
    """
    The engine application: scene container + progressive ray-traced renderer.

    Streaming model: every call to render()/frame() restarts accumulation,
    which is what you want for animation. For a single high-quality still,
    use save(path, samples=N).
    """

    def __init__(self, width=960, height=540, bounces=5, verbose=True):
        self._lib = _load()
        self._ptr = self._lib.ne_create(int(width), int(height))
        if not self._ptr:
            raise RuntimeError("Failed to create native engine instance")
        self.width = width
        self.height = height
        self._materials = []
        self._mat_cache = []
        self._objects = []
        self.verbose = verbose
        self._lib.ne_set_max_bounces(self._ptr, int(bounces))
        if verbose:
            v = self._lib.ne_version().decode()
            print(f"[native-engine] {v} | threads: {self.thread_count}")

    # ------------------------------------------------------------- materials --
    def material(self, albedo=(0.8, 0.8, 0.8), metallic=0.0, roughness=0.5,
                 emissive=None, ior=1.5, transmission=0.0, checker=0.0):
        albedo = Vec3(albedo)
        emissive = Vec3(emissive) if emissive is not None else Vec3(0, 0, 0)
        idx = self._lib.ne_add_material(self._ptr, _f3(albedo), float(metallic),
                                        float(roughness), _f3(emissive), float(ior),
                                        float(transmission), float(checker))
        self._mat_cache.append({
            "albedo": albedo.tuple(), "metallic": float(metallic),
            "roughness": float(roughness), "emissive": emissive.tuple(),
        })
        return Material(self, idx)

    def metal(self, albedo=(0.95, 0.95, 0.95), roughness=0.15):
        return self.material(albedo, metallic=1.0, roughness=roughness)

    def glass(self, tint=(0.97, 0.99, 1.0), ior=1.52):
        return self.material(tint, roughness=0.03, ior=ior, transmission=1.0)

    def emissive(self, color, strength=10.0):
        c = Vec3(color)
        return self.material((0.2, 0.2, 0.2), roughness=1.0,
                             emissive=(c.x * strength, c.y * strength, c.z * strength))

    # -------------------------------------------------------------- geometry --
    def sphere(self, mat, pos=(0, 1, 0), radius=1.0) -> Object:
        idx = self._lib.ne_add_sphere(self._ptr, mat.index, _f3(Vec3(pos)), float(radius))
        o = Object(self, idx, "sphere")
        self._objects.append(o)
        return o

    def box(self, mat, pos=(0, 0.5, 0), size=(1, 1, 1), rot_y=0.0) -> Object:
        """size: full extents (sx, sy, sz), or a scalar for a cube."""
        if isinstance(size, (int, float)):
            size = (size, size, size)
        half = Vec3(size[0] * 0.5, size[1] * 0.5, size[2] * 0.5)
        idx = self._lib.ne_add_box(self._ptr, mat.index, _f3(Vec3(pos)), _f3(half),
                                   float(rot_y))
        o = Object(self, idx, "box")
        self._objects.append(o)
        return o

    def quad(self, mat, p0, u, v) -> Object:
        idx = self._lib.ne_add_quad(self._ptr, mat.index, _f3(Vec3(p0)),
                                    _f3(Vec3(u)), _f3(Vec3(v)))
        o = Object(self, idx, "quad")
        self._objects.append(o)
        return o

    def ground(self, albedo=(0.85, 0.83, 0.80), roughness=0.35, checker=0.0) -> Object:
        m = self.material(albedo, roughness=roughness, checker=checker)
        idx = self._lib.ne_add_plane(self._ptr, m.index, 0.0)
        o = Object(self, idx, "plane")
        self._objects.append(o)
        return o

    def mesh(self, mat, vertices, indices, pos=(0, 0, 0), rot_y=0.0) -> Object:
        import array
        va = array.array("f", [float(v) for v in vertices])
        ia = array.array("I", [int(i) for i in indices])
        nv = len(va) // 3
        ni = len(ia)
        idx = self._lib.ne_add_mesh(self._ptr, mat.index,
                                    (ctypes.c_float * len(va)).from_buffer(va), nv,
                                    (ctypes.c_uint * len(ia)).from_buffer(ia), ni)
        o = Object(self, idx, "mesh")
        self._objects.append(o)
        if pos != (0, 0, 0) or rot_y:
            o.set_transform(pos, rot_y)
        return o

    def torus(self, mat, R=1.0, r=0.35, segments=64, ring_segments=32,
              pos=(0, 1, 0), rot_y=0.0, tilt_x=0.0) -> Object:
        """Procedural torus mesh (tilt in degrees around X, baked at build)."""
        import math
        verts = []
        idxs = []
        tilt = math.radians(tilt_x)
        ct, st = math.cos(tilt), math.sin(tilt)
        for i in range(segments + 1):
            u = 2 * math.pi * i / segments
            cu, su = math.cos(u), math.sin(u)
            for j in range(ring_segments + 1):
                v = 2 * math.pi * j / ring_segments
                x = (R + r * math.cos(v)) * cu
                y = r * math.sin(v)
                z = (R + r * math.cos(v)) * su
                y2 = y * ct - z * st
                z2 = y * st + z * ct
                verts += [x, y2, z2]
        W = ring_segments + 1
        for i in range(segments):
            for j in range(ring_segments):
                a = i * W + j
                b = (i + 1) * W + j
                idxs += [a, b, a + 1, b, b + 1, a + 1]
        return self.mesh(mat, verts, idxs, pos, rot_y)

    # ---------------------------------------------------------- environment --
    def camera(self, pos=(0, 2, 6), target=(0, 0, 0), fov=50.0,
               aperture=0.0, focus_dist=None):
        pos, target = Vec3(pos), Vec3(target)
        if focus_dist is None:
            d = target - pos
            focus_dist = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
        self._lib.ne_set_camera(self._ptr, _f3(pos), _f3(target), float(fov),
                                float(aperture), float(focus_dist))

    def sky(self, top=(0.25, 0.45, 0.85), horizon=(0.9, 0.72, 0.58), intensity=1.0):
        self._lib.ne_set_sky(self._ptr, _f3(Vec3(top)), _f3(Vec3(horizon)),
                             _f3(Vec3((0.42, 0.7, 0.3))), _f3(Vec3((1.0, 0.93, 0.82))),
                             4.5, 1.5, float(intensity))

    def sun(self, direction=(0.42, 0.7, 0.3), color=(1.0, 0.93, 0.82),
            intensity=4.5, radius_deg=1.5):
        d = Vec3(direction).normalized()
        self._lib.ne_set_sky(self._ptr,
                             _f3(Vec3((0.25, 0.45, 0.85))),
                             _f3(Vec3((0.9, 0.72, 0.58))),
                             _f3(d), _f3(Vec3(color)),
                             float(intensity), float(radius_deg), 1.0)

    # -------------------------------------------------------------- rendering --
    def render(self, samples=1) -> float:
        """Render one streaming frame; returns frame time in ms."""
        self._lib.ne_set_streaming(self._ptr, 1)
        return self._lib.ne_render(self._ptr, int(samples))

    def accumulate(self, samples=64, progress=False):
        """Accumulate `samples` into a single high-quality frame."""
        self._lib.ne_set_streaming(self._ptr, 0)
        batch = 16
        done = 0
        while done < samples:
            n = min(batch, samples - done)
            self._lib.ne_render(self._ptr, n)
            done += n
            if progress:
                pct = 100 * done // samples
                sys.stdout.write(f"\r[native-engine] samples {done}/{samples} ({pct}%)  ")
                sys.stdout.flush()
        if progress:
            sys.stdout.write("\n")

    def frame_png(self) -> bytes:
        n = ctypes.c_int(0)
        p = self._lib.ne_frame_png(self._ptr, ctypes.byref(n))
        if not p or n.value == 0:
            return b""
        return ctypes.string_at(p, n.value)

    def frame_rgba(self) -> bytes:
        """Raw tonemapped RGBA8 bytes (width*height*4)."""
        p = self._lib.ne_frame_rgba(self._ptr)
        if not p:
            return b""
        return ctypes.string_at(p, self.width * self.height * 4)

    def save(self, path, samples=64, progress=True):
        """Accumulate `samples` and write a PNG."""
        self.accumulate(samples, progress=progress)
        data = self.frame_png()
        with open(path, "wb") as f:
            f.write(data)
        if self.verbose:
            print(f"[native-engine] wrote {path} ({len(data)} bytes)")
        return path

    # ------------------------------------------------------------------ stats --
    @property
    def thread_count(self):
        return self._lib.ne_stat_threads(self._ptr)

    @property
    def last_frame_ms(self):
        return self._lib.ne_stat_ms(self._ptr)

    @property
    def last_mrays_per_sec(self):
        return self._lib.ne_stat_mrays(self._ptr)

    @property
    def accumulated_samples(self):
        return self._lib.ne_stat_accum(self._ptr)

    @property
    def version(self):
        return self._lib.ne_version().decode()

    def resize(self, width, height):
        self.width, self.height = int(width), int(height)
        self._lib.ne_resize(self._ptr, self.width, self.height)

    def close(self):
        if getattr(self, "_ptr", None):
            self._lib.ne_destroy(self._ptr)
            self._ptr = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

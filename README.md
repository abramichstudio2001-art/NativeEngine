# Native Engine

A **3D game engine core with a real-time ray-traced RTX editor UI**, written
from scratch:

- **Desktop app, zero install** — `bin/NativeEngine.exe` is a self-contained
  Windows executable (300 KB, system DLLs only). It opens the **RTX Editor**:
  native window, streaming path-traced viewport, outliner, material &
  transform inspector, sky/sun controls, render settings, snapshot export.
- **C++17 core** — path tracer (GI, metals, refractive glass, soft shadows,
  emissive lights, BVH, multithreaded, ~8 Mrays/s) wrapped in an
  **interactive streaming renderer**: the viewport never blocks — each frame
  spends a few milliseconds rendering whatever 32px tiles fit the budget,
  tonemaps the partial state, **bilateral-dennoises** and upscales.
  Interaction shows lit ray-traced motion at UI framerate while quality
  converges underneath — the "RTX feels fast" model.
- **Python runtime** — `native_engine` package: builds or reuses the prebuilt
  library, scripting API for scenes/materials/camera, live browser preview,
  and `ne.run_editor()` to launch the native app from Python.

No SDL, no Qt, no NumPy, no pip requirements — just Python 3 and (optionally)
a C++17 compiler to rebuild from source.

```
Double-click  bin\NativeEngine.exe        <- RTX Editor desktop app (Windows)
python3 main.py --editor                  <- same app via the Python runtime
python3 main.py                           <- live browser preview of the showcase
python3 main.py --still --samples 128     <- high-quality cinematic stills
```

![Native Engine RTX Editor](examples/editor_screen.png)

**Editor controls:** LMB pick object · RMB orbit · wheel zoom (viewport) /
scroll inspector (right panel) · sliders edit transforms, materials
(albedo/metallic/roughness/ior/glass/emission/checker), sun & sky, exposure,
RT resolution scale, target SPP, frame budget, denoise, auto-orbit ·
`SNAPSHOT` writes a converged full-res PNG · `SAVE/LOAD` persist `scene.nes`.

## Quick start

```bash
# 1) build the core + start the live RT preview in your browser
python3 main.py
#    -> http://localhost:8000  (progressive path-traced frames, updating live)

# 2) or render a single high-quality still
python3 main.py --still --samples 128 --out showcase.png

# 3) or render a short animation
python3 main.py --frames 48 --samples 32
```

## Writing scenes in Python

```python
import native_engine as ne

app = ne.App(width=1280, height=720)          # compiles the C++ core on demand

# materials
m_floor = app.material((0.85, 0.83, 0.80), roughness=0.3, checker=1.2)
m_gold  = app.metal((1.0, 0.71, 0.29), roughness=0.15)
m_glass = app.glass(ior=1.52)
m_light = app.emissive((1.0, 0.6, 0.3), strength=13.0)

# geometry
app.ground(checker=1.2)
app.sphere(m_gold,  pos=(-1.8, 0.8, 0.4), radius=0.8)
app.sphere(m_glass, pos=(0.6, 1.0, -0.3), radius=1.0)
app.torus(m_gold, R=1.6, r=0.24, pos=(0.6, 1.0, -0.3), tilt_x=-28)  # Saturn ring
app.quad(m_light, p0=(-5, 0.4, -2), u=(0, 2.6, 0), v=(0, 0, 4))     # area light
app.mesh(m_gold, vertices, indices)                                 # your own mesh

# camera & light
app.camera(pos=(0, 2.2, 6.0), target=(0, 0.9, 0), fov=50, aperture=0.05)  # DOF
app.sun(direction=(0.42, 0.7, 0.3), intensity=5.0)
app.sky(top=(0.25, 0.45, 0.85), horizon=(0.9, 0.72, 0.58))

# output
app.save("beauty.png", samples=256)           # accumulate + write PNG

# or live-preview a game loop
srv, port = ne.server.start(app, spp=2, port=8000)
import math, time
t = 0
while True:
    t += 0.05
    app.camera(pos=(6*math.sin(t), 2.4, 6*math.cos(t)), target=(0, 1, 0))
    time.sleep(0.03)                          # preview re-renders per browser frame
```

## Python API summary

| call | purpose |
|---|---|
| `App(width, height, bounces=5)` | create engine (auto-builds native lib) |
| `app.material / metal / glass / emissive` | create materials (`Material` handle) |
| `app.sphere / box / quad / plane(ground) / mesh / torus` | geometry (`Object` handle) |
| `obj.set_transform(pos, rot_y)` | move/rotate any object per frame |
| `mat.update(albedo=..., emissive=...)` | live material animation |
| `app.camera(pos, target, fov, aperture, focus_dist)` | perspective + depth-of-field |
| `app.sun(...) / app.sky(...)` | analytic sky + visible sun disk |
| `app.render(spp)` | one streaming frame (progressive preview) |
| `app.save(path, samples=N)` | accumulate N spp, write PNG |
| `app.frame_png() / frame_rgba()` | grab the frame for your own pipeline |
| `app.last_frame_ms / mrays_per_sec / thread_count` | perf stats |
| `ne.server.start(app, ...)` | zero-dependency live browser preview |

## Architecture

```
bin/                       prebuilt NativeEngine.exe / NativeEngine.dll / .so
engine/src/                C++ core (header-mostly, single .cpp for the ABI)
  ne_math.h                Vec3/Mat4/AABB, PCG32 RNG, sampling helpers
  ne_bvh.h                 surface-area-BVH over triangles (two-finger walk)
  ne_scene.h               objects (sphere/box/quad/plane/mesh), materials,
                           analytic sky+sun, shadow transmittance
  ne_renderer.h            path tracer: MIS-style NEE, GGX-rough metals,
                           Schlick Fresnel dielectrics w/ TIR, RR, firefly
                           clamp, tiled thread-pool, ACES filmic, PNG encoder
  ne_api.h / ne_api.cpp    stable C ABI consumed by Python via ctypes
  ne_demo.h                the C++ showcase scene (used by the CLI)
  ne_cli.cpp               standalone C++ render tool (--help)
native_engine/             Python package
  build.py                 detects g++/clang++/MSVC, builds build/libNativeEngine.*
  engine.py                ctypes bindings + high-level App/Material/Object API
  server.py                live preview HTTP server (stdlib only)
main.py                    runner: editor | browser preview | still | animation
```

The engine is a **path tracer with progressive refinement**: each frame adds
samples to a running accumulation buffer, so a live view becomes cleaner every
second — the same model used by production RT renderers. Objects carry
transform matrices; transformed meshes refit their world-space triangles and
rebuild their BVH (`O(n log n)`, fine for a few animated props per frame).
Meshes are indexed triangles only.

### Limitations (deliberate v1 scope)
- CPU path tracing, not a rasterized GPU pipeline — frames converge progressively.
- One directional sun + quad area lights; no spot/point light objects yet.
- No textures, shadows from meshes are single-sided-safe via two-sided normals.
- **Windows**: any MinGW-w64 g++ works — including "win32 threads model"
builds where `std::thread` fails to link (`collect2.exe: error: ld returned
1`); the engine spawns threads through the native Win32 API instead and
static-links the C++ runtime, so no extra DLLs are needed on PATH. MSVC
(`cl`) also works from a Developer Command Prompt. Build paths on Windows
are smoke-tested via a stubbed Windows API; if you hit an issue, delete
`build/` and rerun — the exact compiler error is now shown in the traceback.

## CLI (C++ only, no Python needed)

```bash
g++ -std=c++17 -O3 -ffast-math -pthread -Iengine/src \
    engine/src/ne_cli.cpp engine/src/ne_api.cpp -o ne_cli
./ne_cli --w 1280 --h 720 --spp 64 --angle 20 --out render.png
```

## License
MIT — do whatever you like with it.

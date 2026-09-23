#!/usr/bin/env python3
# ============================================================================
#  Native Engine — main.py
#  Runs the real-time ray-traced showcase:
#      python3 main.py                 # live preview server (watch in browser)
#      python3 main.py --still         # one high-quality PNG, no server
#      python3 main.py --frames 240    # render an animation to out/frame_*.png
# ============================================================================
import argparse
import math
import os
import sys
import time

import native_engine as ne


# ----------------------------------------------------------------------------
#  The showcase scene, built entirely through the Python game-engine API.
# ----------------------------------------------------------------------------
def build_showcase(app: ne.App):
    # ---- materials -----------------------------------------------------------
    m_ground = app.material((0.85, 0.83, 0.80), roughness=0.32, checker=1.25)
    m_gold = app.metal((1.00, 0.71, 0.29), roughness=0.14)
    m_glass = app.glass((0.97, 0.99, 1.00), ior=1.52)
    m_red = app.material((0.78, 0.12, 0.10), roughness=0.85)
    m_ring = app.metal((0.95, 0.96, 0.98), roughness=0.22)
    m_magenta = app.emissive((1.0, 0.16, 0.95), strength=8.5)
    m_cyan = app.emissive((0.11, 0.72, 0.95), strength=9.0)
    m_bulb = app.emissive((1.0, 0.60, 0.30), strength=13.0)

    # ---- geometry ------------------------------------------------------------
    app.ground(checker=1.25)
    glass = app.sphere(m_glass, pos=(0.0, 1.05, -0.2), radius=1.0)
    ring = app.torus(m_ring, R=1.58, r=0.235, segments=96, ring_segments=40,
                     pos=(0.0, 1.05, -0.2), tilt_x=-28.0)
    gold = app.sphere(m_gold, pos=(-2.6, 0.78, 1.2), radius=0.78)
    red = app.sphere(m_red, pos=(2.5, 0.68, 1.0), radius=0.68)
    app.quad(m_magenta, p0=(-5.6, 0.4, -2.0), u=(0, 2.6, 0), v=(0, 0, 4.0))
    app.quad(m_cyan, p0=(3.2, 0.4, -5.6), u=(3.2, 0, 0), v=(0, 2.6, 0))
    bulb = app.sphere(m_bulb, pos=(0.0, 4.4, 2.6), radius=0.30)

    app.sun(direction=(0.42, 0.55, 0.28), color=(1.0, 0.90, 0.78),
            intensity=4.5, radius_deg=1.5)
    app.sky(top=(0.25, 0.45, 0.85), horizon=(0.90, 0.72, 0.58), intensity=1.0)

    return {"glass": glass, "ring": ring, "gold": gold, "red": red, "bulb": bulb}


def animate(app, objs, t, base_positions):
    """One animation step: orbiting hero camera + gently moving props."""
    # camera: slow cinematic orbit around the glass saturn
    a = math.radians(t * 12.0)
    px = 6.4 * math.sin(a)
    pz = 6.4 * math.cos(a)
    py = 2.35 - 0.35 * math.cos(a * 0.5)
    app.camera(pos=(px, py, pz), target=(0.0, 0.95, -0.2), fov=50.0,
               aperture=0.055, focus_dist=6.6)

    # gold sphere: gentle bob
    x, y0, z = base_positions["gold"]
    objs["gold"].set_transform(pos=(x, y0 + 0.12 * math.sin(t * 2.1), z))

    # red sphere: lazy orbit
    x, y0, z = base_positions["red"]
    objs["red"].set_transform(pos=(x + 0.45 * math.sin(t * 0.8),
                                   y0, z + 0.45 * math.cos(t * 0.8)))

    # ring: slow spin around the glass sphere (mesh BVH rebuilds on transform)
    objs["ring"].set_transform(pos=(0.0, 1.05, -0.2), rot_y=t * 24.0)

    # light bulb drifts
    objs["bulb"].set_transform(pos=(1.2 * math.sin(t * 0.6), 4.4, 2.6))


# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Native Engine — ray-traced showcase")
    ap.add_argument("--width", type=int, default=960)
    ap.add_argument("--height", type=int, default=540)
    ap.add_argument("--spp", type=int, default=2, help="samples per pixel per frame")
    ap.add_argument("--bounces", type=int, default=5)
    ap.add_argument("--port", type=int, default=8000, help="0 = pick a free port")
    ap.add_argument("--still", action="store_true", help="render one PNG and exit")
    ap.add_argument("--samples", type=int, default=128, help="samples for --still")
    ap.add_argument("--frames", type=int, default=0, help="render N PNG frames and exit")
    ap.add_argument("--out", default="render.png", help="output path for --still")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    t0 = time.time()
    app = ne.App(width=args.width, height=args.height, bounces=args.bounces,
                 verbose=not args.quiet)
    objs = build_showcase(app)
    base = {"gold": (-2.6, 0.78, 1.2), "red": (2.5, 0.68, 1.0)}
    if not args.quiet:
        print(f"[native-engine] ready in {time.time() - t0:.1f}s "
              f"({len(objs)} animated props)")

    if args.still:
        animate(app, objs, 1.5, base)
        out = app.save(args.out, samples=args.samples, progress=not args.quiet)
        print(out)
        return

    if args.frames > 0:
        os.makedirs("out", exist_ok=True)
        for f in range(args.frames):
            animate(app, objs, f / 24.0, base)
            app.save(f"out/frame_{f:04d}.png", samples=max(8, args.samples // 4),
                     progress=False)
            if not args.quiet:
                print(f"[native-engine] frame {f + 1}/{args.frames}")
        return

    # ---- live preview server -------------------------------------------------
    srv, port = ne.server.start(app, spp=max(1, args.spp), interval_ms=60,
                                port=args.port, title="Native Engine — RTX Showcase")
    if not args.quiet:
        print()
        print("=" * 62)
        print("  Native Engine live preview")
        print(f"  http://localhost:{port}")
        print("  (Ctrl+C to stop)")
        print("=" * 62)
        print()
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        if not args.quiet:
            print("\n[native-engine] shutting down.")
    finally:
        srv.shutdown()
        app.close()


if __name__ == "__main__":
    main()

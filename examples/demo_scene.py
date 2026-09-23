#!/usr/bin/env python3
# ============================================================================
#  Native Engine demo — a spinning metal cube + glass sphere over an
#  emissive-lit checkered floor, shown live in the browser.
#  Run:  python3 examples/demo_scene.py
# ============================================================================
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import native_engine as ne  # noqa: E402

app = ne.App(width=800, height=450, bounces=4)

app.ground(albedo=(0.82, 0.82, 0.85), roughness=0.25, checker=1.1)

m_chrome = app.metal((0.95, 0.96, 1.0), roughness=0.18)
m_glass = app.glass()
m_green = app.emissive((0.25, 1.0, 0.45), strength=10.0)
m_rose = app.emissive((1.0, 0.35, 0.55), strength=8.0)

cube = app.box(m_chrome, pos=(0.0, 0.85, 0.0), size=1.3, rot_y=20.0)
orb = app.sphere(m_glass, pos=(1.9, 1.0, -0.6), radius=1.0)
app.quad(m_green, p0=(-4.6, 0.3, -1.6), u=(0, 2.4, 0), v=(0, 0, 3.6))
app.quad(m_rose, p0=(2.6, 0.3, -4.6), u=(2.6, 0, 0), v=(0, 2.4, 0))

app.sun(direction=(-0.35, 0.8, 0.2), intensity=3.0, color=(1.0, 0.95, 0.88))

srv, port = ne.server.start(app, spp=2, title="Native Engine — Demo")
print(f"demo live at http://localhost:{port}  (Ctrl+C to quit)")

t = 0.0
try:
    while True:
        t += 0.08
        cube.set_transform(pos=(0.0, 0.9 + 0.18 * math.sin(t * 2.0), 0.0),
                           rot_y=math.degrees(t) * 1.5)
        orb.set_transform(pos=(1.9 + 0.5 * math.sin(t * 0.7), 1.0, -0.6))
        ang = t * 0.35
        app.camera(pos=(5.4 * math.sin(ang), 2.6, 5.4 * math.cos(ang)),
                   target=(0.3, 0.9, -0.2), fov=48.0, aperture=0.06)
        time.sleep(0.02)
except KeyboardInterrupt:
    srv.shutdown()
    app.save("demo_still.png", samples=96)
    print("saved demo_still.png")

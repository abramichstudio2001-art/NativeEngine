# NEON ORBS -- RTX game sample for the Native Engine IDE.
# Press RUN (Ctrl+R): this script streams a path-traced game in a native
# window. Ray tracing is progressive: the picture starts noisy and sharpens
# while you play, and dragging objects never stalls the frame.
import os, sys, math, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
import native_engine as ne

RTW, RTH = 480, 270          # trace at low res, upscale -> instant feedback
WINW, WINH = 960, 540

app = ne.App(RTW, RTH, bounces=3)
app.ground(albedo=(0.82, 0.84, 0.9), roughness=0.28, checker=0.55)
floor_mat = app.emissive((0.1, 0.9, 1.0), 4.0)
app.quad(floor_mat, (-6, 0.01, -6), (12, 0, 0), (0, 0, 12))
mats = [app.metal((1.0, 0.72, 0.25), 0.12),
        app.metal((0.9, 0.25, 0.9), 0.05),
        app.glass((0.97, 0.99, 1.0), 1.52),
        app.metal((0.3, 0.95, 0.5), 0.2)]
balls = [app.sphere(mats[i], ((i - 1.5) * 2.2, 1.1, -2.0 + (i % 2))) for i in range(4)]

win = ne.Window(WINW, WINH, "NEON ORBS - Native Engine RTX")
t0 = time.time()
app.begin_stream(32)
while win.pump():
    t = time.time() - t0
    for i, b in enumerate(balls):
        x = (i - 1.5) * 2.2
        z = -2.0 + math.cos(t * (0.7 + 0.2 * i)) * 2.5
        y = 1.1 + abs(math.sin(t * (1.4 + 0.15 * i))) * 1.3
        b.set_transform(pos=(x, y, z))
    app.camera(pos=(math.sin(t * 0.15) * 6.0, 2.4, math.cos(t * 0.15) * 6.0),
               target=(0, 1.0, -1))
    app.begin_stream(32)          # edit -> re-queue the tile stream
    busy, passes = app.stream_frame(4)
    win.present(app.blit(WINW, WINH))
    print(f"\rframe {t:6.1f}s  passes={passes:2d}  busy={busy}", end="")
    if win.key_pressed(ne.K_ESCAPE):
        break
print("\nbye - Native Engine")

#!/usr/bin/env python3
# ============================================================================
#  Native Engine — desktop game example (pure Python game code, C++ engine)
#  "CRYSTAL RUSH": collect glowing crystals, dodge seekers.
#   * Windows : opens a real 960x540 window, 60 fps (WASD + SPACE, RMB orbit)
#   * Linux/mac: runs a scripted headless demo (for CI) and writes PNGs
#      python3 examples/game_desktop.py [headless_frames]
# ============================================================================
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import native_engine as ne  # noqa: E402

IS_WIN = sys.platform.startswith("win")
FRAMES = 0 if IS_WIN else (int(sys.argv[1]) if len(sys.argv) > 1 else 300)
W, H = 960, 540
N_CR, N_EN, ARENA = 8, 3, 14.0

app = ne.App(W, H, verbose=False)
win = ne.Window(W, H, "Native Engine - CRYSTAL RUSH (python)")

# ---------------------------------------------------------------- scene ------
app.ground(albedo=(0.80, 0.80, 0.86), roughness=0.4, checker=1.7)
m_player = app.metal((1.0, 0.72, 0.25), roughness=0.2)
m_crystal = app.material((0.2, 1.0, 0.45), roughness=0.3, emissive=(0.15, 1.3, 0.45))
m_enemy = app.material((0.9, 0.1, 0.15), metallic=0.6, roughness=0.4,
                       emissive=(0.7, 0.05, 0.05))
m_wall = app.material((0.06, 0.07, 0.12), roughness=0.4)

player = app.sphere(m_player, pos=(0, 0.7, 5), radius=0.7)
crystals = [app.sphere(m_crystal, pos=(i * 3.1 - 8, 0.55, (i * 5.7) % 21 - 10), radius=0.38)
            for i in range(N_CR)]
enemies = [app.sphere(m_enemy, pos=(i * 6.0 - 9, 0.9, i * 3.7 - 6), radius=0.55)
           for i in range(N_EN)]
for s in (-1, 1):
    app.box(m_wall, pos=(s * ARENA, 1.5, 0), size=(1.0, 3.0, 2 * ARENA + 2))
    app.box(m_wall, pos=(0, 1.5, s * ARENA), size=(2 * ARENA + 2, 3.0, 1.0))
app.sun(direction=(0.35, 0.5, -0.6), intensity=2.8, color=(1.0, 0.7, 0.55))
app.sky(top=(0.06, 0.08, 0.18), horizon=(0.85, 0.4, 0.45), intensity=0.9)
app.raster_resize()

# ------------------------------------------------------------ game state -----
ppos = [0.0, 0.7, 5.0]          # player position (single source of truth)
pvel = [0.0, 0.0, 0.0]
cpos = [[c._pos if False else ppos[0], 0, 0] for c in crystals]  # placeholder, fixed below
cpos = [[i * 3.1 - 8, 0.55, (i * 5.7) % 21 - 10] for i in range(N_CR)]
alive = [True] * N_CR
epos = [[i * 6.0 - 9, 0.9, i * 3.7 - 6] for i in range(N_EN)]
score, lives, t = 0, 3, 0.0
camera_yaw = -0.5
grace = 0.0

print("Native Engine | CRYSTAL RUSH — pure-python game on the C++ core")
print(f"  engine: {app.version} | threads: {app.thread_count} | "
      f"{'desktop window' if IS_WIN else 'headless demo'}")

prev = time.time()
frame = 0
while True:
    if not win.pump():
        break
    dt = min(0.05, time.time() - prev)
    prev = time.time()
    t += dt
    grace = max(0.0, grace - dt)

    # ---- input -------------------------------------------------------------
    if IS_WIN:
        fwd = (math.sin(camera_yaw), math.cos(camera_yaw))
        ax = (1 if win.key_down(ord('W')) else 0) - (1 if win.key_down(ord('S')) else 0)
        az = (1 if win.key_down(ord('D')) else 0) - (1 if win.key_down(ord('A')) else 0)
        mx = ax * fwd[0] - az * fwd[1]
        mz = ax * fwd[1] + az * fwd[0]
        if win.mouse()[5]:
            camera_yaw -= win.mouse()[2] * 0.005
    else:  # scripted: circle the arena, headless mode
        ang = frame * 0.035
        mx, mz = math.sin(ang), math.cos(ang)
        camera_yaw = ang + math.pi

    # ---- player physics ------------------------------------------------------
    pvel[0] = (pvel[0] + mx * 44 * dt) * math.pow(0.002, dt)
    pvel[2] = (pvel[2] + mz * 44 * dt) * math.pow(0.002, dt)
    if IS_WIN and win.key_down(0x20) and ppos[1] <= 0.701:
        pvel[1] = 11.0
    pvel[1] -= 32 * dt
    for i in range(3):
        ppos[i] += pvel[i] * dt
    if ppos[1] < 0.7:
        ppos[1] = 0.7
        pvel[1] = -pvel[1] * 0.35 if pvel[1] < -6 else 0.0
    lim = ARENA - 1.4
    for i in (0, 2):
        if ppos[i] < -lim: ppos[i], pvel[i] = -lim, abs(pvel[i]) * 0.3
        if ppos[i] > lim:  ppos[i], pvel[i] = lim, -abs(pvel[i]) * 0.3
    player.set_transform(pos=tuple(ppos))

    # ---- crystals: bob + collect --------------------------------------------
    for i, cp in enumerate(cpos):
        cp[1] = 0.55 + 0.2 * math.sin(t * 2.6 + i)
        if alive[i] and math.hypot(cp[0] - ppos[0], cp[2] - ppos[2]) < 1.35:
            alive[i] = False
            score += 1
            cp[0], cp[2] = (i * 4.3 + score * 5.1) % (2 * lim) - lim, \
                           (i * 7.9 + score * 3.7) % (2 * lim) - lim
        crystals[i].set_transform(pos=tuple(cp) if alive[i] else (0, -9999, 0))

    # ---- seekers --------------------------------------------------------------
    if score >= N_CR:
        break
    for i, ep in enumerate(epos):
        spd = 3.4 + 0.4 * score
        dx, dz = ppos[0] - ep[0], ppos[2] - ep[2]
        d = math.hypot(dx, dz) or 1.0
        ep[0] += dx / d * spd * dt
        ep[2] += dz / d * spd * dt
        ep[1] = 0.9 + 0.4 * math.sin(t * 3.0 + i * 2)
        if grace <= 0 and d < 1.25:
            lives -= 1
            grace = 1.5
            pvel[0] += dx / d * -8; pvel[2] += dz / d * -8; pvel[1] = 7
            if lives <= 0:
                break
        enemies[i].set_transform(pos=tuple(ep))
    if lives <= 0:
        break

    # ---- camera + render -------------------------------------------------------
    eye = (ppos[0] + 7.2 * math.sin(camera_yaw), ppos[1] + 3.6,
           ppos[2] + 7.2 * math.cos(camera_yaw))
    app.raster_render(eye, (ppos[0], ppos[1] + 0.6, ppos[2]), 62.0)
    msg = "YOU WIN!" if score >= N_CR else ("CAPTURED!" if lives <= 0 else
                                            f"CRYSTAL RUSH   SCORE {score}/{N_CR}")
    app.hud_text(14, 12, msg, 3, 0xFFD24A if score >= N_CR else 0xFFFFFF)
    app.hud_text(14, 32, f"TIME {t:4.1f}   LIVES {max(lives,0)}   "
                         + ("WASD SPACE RMB" if IS_WIN else f"FRAME {frame}"), 2, 0x99AACC)
    buf, w, h = app.raster_pixels()
    if w:
        win.present(buf)
    frame += 1
    if FRAMES and frame >= FRAMES:
        break

# save a beauty shot through the real path tracer as proof both renderers work
app.save("/tmp/crush_rt.png", samples=24, progress=False)
win.close()
print(f"done: frames={frame} score={score} lives={max(lives,0)} "
      f"| RT beauty shot: /tmp/crush_rt.png")

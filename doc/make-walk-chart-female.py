"""
Male walk-cycle chart + a "what changes for female" delta sheet, for the floormat NPCs.

Two outputs:
  doc/walk-cycle-male.png          full male reference (all channels)
  doc/walk-cycle-female-delta.png  ONLY the axes that differ male->female

Sex differences are grounded in gait literature (see doc note at bottom). The key
point: the SAGITTAL leg timing (thigh pitch / knee / foot pitch) is sex-shared and
does NOT change. The differences live in the pelvis (frontal + transverse + sagittal
tilt), stance width, and arm-swing amplitude. Numbers:

  - Pelvic obliquity (frontal roll):  male 7.4 deg range,  female 9.4 deg  (Kerrigan 2002)
  - Pelvic transverse rotation (yaw):  female > male
  - Anterior pelvic tilt:              female > male
  - Arm swing + torso rotation:        female > male
  - Step width / hip abduction:        male wider, female narrower

Shapes here are schematic timing references, not motion-capture curves.
"""

import math
from PIL import Image, ImageDraw, ImageFont

SS = 2
FMAX = 20

BG    = (250, 250, 248)
INK   = (30, 32, 36)
GRID  = (224, 224, 220)
KEYLN = (148, 152, 160)
ZERO  = (198, 200, 196)
C_LEFT  = (224, 132, 44)                # left side  (orange)
C_RIGHT = (120, 96, 188)                # right side (purple)
C_BOB   = (208, 70, 60)
C_SWAY  = (44, 122, 196)
C_PELV  = (40, 158, 110)                # pelvis channels (green)
C_GREY  = (150, 150, 156)
C_MALE  = (96, 108, 150)                # delta sheet: male baseline (slate)
C_FEM   = (210, 78, 150)                # delta sheet: female target (magenta)

# one distinct hue per male-sheet panel, in panel order
PAL_MALE = [
    (210, 68, 58),    # bob        red
    (44, 122, 196),   # sway       blue
    (40, 158, 110),   # obliquity  green
    (0, 148, 168),    # yaw        teal
    (150, 92, 192),   # tilt       purple
    (226, 130, 40),   # thigh      orange
    (206, 72, 148),   # knee       magenta
    (146, 104, 56),   # foot       brown
    (92, 104, 178),   # abduction  indigo
    (176, 58, 92),    # arm        crimson
    (170, 150, 40),   # elbow      olive
]

def font(sz, bold=False):
    name = "arialbd.ttf" if bold else "arial.ttf"
    try:
        return ImageFont.truetype("C:/Windows/Fonts/" + name, sz * SS)
    except OSError:
        return ImageFont.load_default()

F_TITLE = font(28, True)
F_PANEL = font(16, True)
F_SMALL = font(13)
F_KEY   = font(15, True)
F_VAL   = font(12, True)
F_NOTE  = font(13)

KEYS = [(0, "C"), (2, "D"), (5, "P"), (8, "U"),
        (10, "C"), (12, "D"), (15, "P"), (18, "U")]   # 20 == 0, omitted
KEYF = [k for k, _ in KEYS]

# ---- periodic hermite (same as make_walk_chart.py) ------------------------
def periodic_hermite(cps, period=20.0):
    pts = [(float(t), float(v)) for t, v in cps]
    if pts[-1][0] % period == pts[0][0] % period:
        pts = pts[:-1]
    n = len(pts)
    def tangent(i):
        t0, v0 = pts[i]; tm, vm = pts[(i - 1) % n]; tp, vp = pts[(i + 1) % n]
        if tm >= t0: tm -= period
        if tp <= t0: tp += period
        if (vp - v0) * (v0 - vm) <= 0: return 0.0
        return (vp - vm) / (tp - tm)
    def fn(f):
        f = f % period
        for i in range(n):
            t0, v0 = pts[i]; t1, v1 = pts[(i + 1) % n]
            if t1 <= t0: t1 += period
            ff = f if f >= t0 else f + period
            if t0 <= ff <= t1:
                m0, m1 = tangent(i), tangent((i + 1) % n); h = t1 - t0; s = (ff - t0) / h
                return ((2*s**3 - 3*s**2 + 1)*v0 + (s**3 - 2*s**2 + s)*h*m0
                        + (-2*s**3 + 3*s**2)*v1 + (s**3 - s**2)*h*m1)
        return pts[0][1]
    fn.keyframes = sorted({t % period for t, _ in pts})   # the curve's own control points
    return fn

# ---- SEX-SHARED channels (identical male/female) ---------------------------
bob = periodic_hermite([(0, 0.45), (2, 0.0), (5, 1.0), (8, 0.62),
                        (10, 0.45), (12, 0.0), (15, 1.0), (18, 0.62)])
def bob_m(f):  return (bob(f) - 0.45) * 0.05
thigh_L = periodic_hermite([(0, -27), (5, -5), (10, 13), (15, -8)])   # rig-inverted vs anatomical +fwd
knee_L = periodic_hermite([(0, 5), (2, 18), (6, 5), (10, 10),
                           (12, 40), (15, 63), (17, 25), (20, 5)])
foot_L = periodic_hermite([(0, -25), (2, 0), (4, 0), (6, 0), (8, 8),
                           (10, 18), (12, 24), (13, 12), (14, 0),
                           (16, -8), (18, -15), (20, -25)])   # rig-inverted vs anatomical +toes-up

# ---- SEX-VARYING channels (factories: pass the per-sex amplitude) ----------
# lateral hip sway is essentially sex-neutral, kept shared at 4 cm peak-to-peak.
def sway_m(f):  return 0.5 * 0.04 * math.sin(2*math.pi * f / 20)

# obliquity: pelvis drops on the swing-leg side, one full cycle per STRIDE (period 20).
# rig sign confirmed in Blender: +rot X drops the left hip and swings its foot to midline.
# negate the anatomical "+L hip up" so the plotted number is directly typeable as rot X.
def obliquity(rng):                     # rng = peak-to-peak degrees
    return lambda f: -0.5 * rng * math.sin(2*math.pi * f / 20)
def yaw(amp):                           # +-amp deg, peaks at contact
    return lambda f: amp * math.cos(2*math.pi * f / 20)
def hip_frontal(base):                  # frontal hip angle, + = abduction (wider stance)
    # adducts to a narrow peak at midstance (~f=6), abducts in swing for foot clearance
    return lambda f: base - 5.0 * math.cos(2*math.pi * (f - 6) / 20)
def arm_R(amp):                         # shoulder flex/ext, +fwd, slight asymmetry
    return lambda f: 5.0 + amp * math.cos(2*math.pi * f / 20)
def arm_L(amp):
    aR = arm_R(amp)
    return lambda f: 10.0 - aR(f)       # opposes R arm, kept near same band
def elbow_L(base, amp):                 # left forearm flexion, max when the left arm is forward (~f=10)
    return lambda f: base - amp * math.cos(2*math.pi * f / 20)

SEX = {
    "male":   dict(obliq=7.4, yaw=5.0, tilt=7.0,  abd=5.0, arm=18.0, elb_base=18.0, elb_amp=6.0,  carry=7.0,  knee_val=2.0),
    "female": dict(obliq=9.4, yaw=8.0, tilt=12.5, abd=2.0, arm=24.0, elb_base=28.0, elb_amp=10.0, carry=13.0, knee_val=5.0),
}

# ---------------------------------------------------------------------------
# rendering
# ---------------------------------------------------------------------------
def render(path, title, subtitle, panels, footer):
    """panels: list of dicts {title, curves, vmin, vmax, ystep, unit, fill}.
    curves: list of (fn, color, label, dash, dots)."""
    PT, PH, PG = 150, 150, 28            # header, panel height, gap (pre-SS); gap holds the ruler
    W = 1500 * SS
    Lm, Rm = 122 * SS, 60 * SS
    H = (PT + len(panels) * (PH + PG) + 56 + 26 * (len(footer) + 1)) * SS

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    def x_of(f): return Lm + (f / FMAX) * (W - Lm - Rm)

    d.text((Lm, 26*SS), title, font=F_TITLE, fill=INK)
    d.text((Lm, 62*SS), subtitle, font=F_SMALL, fill=(96, 98, 104))

    # key-pose phase row (C/D/P/U only); frame numbers now sit under each panel
    yc = 104 * SS
    for k, lab in KEYS:
        x = x_of(k)
        d.line([(x, yc), (x, yc + 12*SS)], fill=KEYLN, width=2*SS)
        lw = d.textlength(lab, font=F_KEY)
        d.text((x - lw/2, yc - 18*SS), lab, font=F_KEY, fill=INK)

    def vgrid(y0, y1):
        for f in range(FMAX + 1):
            x = x_of(f); key = f in KEYF
            d.line([(x, y0), (x, y1)], fill=(202, 204, 210) if key else (234, 234, 231),
                   width=1*SS)

    def panel(spec, ytop, ybot):
        if spec.get("rows") is not None:        # static-poses table: set-once values, not keyed per-frame
            d.rectangle([Lm, ytop, W - Rm, ybot], outline=GRID, width=1*SS)
            d.text((Lm + 8*SS, ytop + 5*SS), spec["title"], font=F_PANEL, fill=INK)
            ry = ytop + 36*SS
            for lab, val in spec["rows"]:
                d.text((Lm + 16*SS, ry), lab, font=F_SMALL, fill=(70, 72, 78))
                vw = d.textlength(val, font=F_KEY)
                d.text((W - Rm - 16*SS - vw, ry), val, font=F_KEY, fill=INK)
                d.line([(Lm + 12*SS, ry + 21*SS), (W - Rm - 12*SS, ry + 21*SS)],
                       fill=(238, 238, 234), width=1*SS)
                ry += 32*SS
            return
        vmin, vmax = spec["vmin"], spec["vmax"]
        unit = spec.get("unit", ""); ystep = spec.get("ystep")
        d.rectangle([Lm, ytop, W - Rm, ybot], outline=GRID, width=1*SS)
        vgrid(ytop, ybot)
        pad = 15 * SS
        def y_of(v): return ybot - pad - (v - vmin) / (vmax - vmin) * (ybot - ytop - 2*pad)
        if ystep:
            half = ystep / 2.0                       # minor guides halfway between labelled lines
            for ns in range(math.ceil(vmin/half - 1e-9), math.floor(vmax/half + 1e-9) + 1):
                if ns % 2 == 0: continue              # even multiples coincide with major lines below
                yy = y_of(ns * half)
                d.line([(Lm, yy), (W - Rm, yy)], fill=(243, 243, 240), width=1*SS)
            for ns in range(math.ceil(vmin/ystep - 1e-9), math.floor(vmax/ystep + 1e-9) + 1):
                v = ns * ystep; yy = y_of(v); z = abs(v) < 1e-6
                d.line([(Lm, yy), (W - Rm, yy)], fill=ZERO if z else (232, 233, 229),
                       width=2*SS if z else 1*SS)
                lab = f"{v:g}{unit}"; tw = d.textlength(lab, font=F_SMALL)
                d.text((Lm - 9*SS - tw, yy - 8*SS), lab, font=F_SMALL,
                       fill=INK if z else (150, 152, 158))
        if spec.get("fill"):
            fn0 = spec["curves"][0][0]
            pts = [(x_of(f/10), y_of(fn0(f/10))) for f in range(FMAX*10 + 1)]
            d.polygon(pts + [(x_of(FMAX), ybot), (x_of(0), ybot)], fill=spec["fill"])
        if unit == "m":
            vfmt = lambda v: f"{v:+.3f}"
        else:                                   # one decimal so small pelvis deltas stay readable
            vfmt = lambda v: f"{(0.0 if abs(v) < 5e-2 else v):.1f}{unit}"
        for fn, color, label, dash, dots in spec["curves"]:
            pts = [(x_of(f/10), y_of(fn(f/10))) for f in range(FMAX*10 + 1)]
            if dash:
                for i in range(len(pts) - 1):
                    if (i // dash) % 2 == 0:
                        d.line([pts[i], pts[i+1]], fill=color, width=2*SS)
            else:
                d.line(pts, fill=color, width=3*SS, joint="curve")
        # dots + value labels: at each frame the higher value labels above its dot, the lower
        # below, so overlaid curves push their numbers apart instead of writing over each other.
        # value labels sit at each curve's OWN keyframes (control points); sinusoidal curves
        # have none, so they fall back to the global pose grid.
        grid_kfs = [k for k in KEYF if k != FMAX]
        events = {}
        for ci, (fn, color, _, _, dots) in enumerate(spec["curves"]):
            if not dots: continue
            for k in getattr(fn, "keyframes", grid_kfs):
                events.setdefault(k, []).append((ci, fn, color))
        labels = []
        for k in sorted(events):
            ranked = sorted(((fn(k), ci, fn, color) for ci, fn, color in events[k]),
                            key=lambda e: (e[0], e[1]), reverse=True)
            for rank, (v, ci, fn, color) in enumerate(ranked):
                x, y = x_of(k), y_of(v)
                d.ellipse([x-4*SS, y-4*SS, x+4*SS, y+4*SS], fill=color, outline=BG, width=1*SS)
                s = vfmt(v); tw = d.textlength(s, font=F_VAL)
                above = (len(ranked) > 1 and rank == 0)
                ty = (y - 20*SS) if above else (y + 7*SS)
                if ty < ytop + 22*SS:        ty = y + 7*SS   # keep labels out of the title band
                if ty + 15*SS > ybot - 2*SS: ty = y - 20*SS
                tx = min(max(x - tw/2, Lm + 3*SS), W - Rm - tw - 3*SS)  # keep label inside the panel
                labels.append((tx, ty, tw, s, color))
        # push apart labels that share horizontal space and overlap vertically (close
        # male/female pairs near a peak, where the upper label can't go above the title).
        labels.sort(key=lambda L: (L[1], L[0]))
        placed = []
        for idx, (tx, ty, tw, s, color) in enumerate(labels):
            for px, py, pw in placed:
                if (tx < px + pw + 2*SS and px < tx + tw + 2*SS
                        and ty < py + 16*SS and py < ty + 16*SS):
                    ty = py + 16*SS
            labels[idx] = (tx, ty, tw, s, color)
            placed.append((tx, ty, tw))
        # opaque background (panel colour) under every label first, then all text on top, so a
        # neighbouring label's box can never paint over another label's digits.
        for tx, ty, tw, *_ in labels:
            d.rectangle([tx - 3*SS, ty - 1*SS, tx + tw + 3*SS, ty + 15*SS], fill=BG)
        for tx, ty, tw, s, _ in labels:
            d.text((tx, ty), s, font=F_VAL, fill=INK)
        # per-panel frame ruler: tick + number every frame this panel labels (its own keyframes,
        # or the pose grid for sinusoidal curves), so each graph can be read on its own.
        kfs = sorted({k for fn, _, _, _, dots in spec["curves"] if dots
                      for k in getattr(fn, "keyframes", grid_kfs)})
        for k in kfs:
            x = x_of(k)
            d.line([(x, ybot + 2*SS), (x, ybot + 8*SS)], fill=KEYLN, width=1*SS)
            num = f"{k:g}"; nw = d.textlength(num, font=F_VAL)
            d.text((x - nw/2, ybot + 9*SS), num, font=F_VAL, fill=(110, 114, 122))
        d.text((Lm + 8*SS, ytop + 5*SS), spec["title"], font=F_PANEL, fill=INK)
        lx = W - Rm - 12*SS
        for fn, color, label, dash, dots in reversed(spec["curves"]):
            if not label: continue
            tw = d.textlength(label, font=F_SMALL); lx -= tw
            d.text((lx, ytop + 7*SS), label, font=F_SMALL, fill=color)
            lx -= 34*SS; yl = ytop + 15*SS
            d.line([(lx + 4*SS, yl), (lx + 26*SS, yl)], fill=color, width=3*SS)
            lx -= 14*SS

    for i, spec in enumerate(panels):
        t = (PT + i * (PH + PG)) * SS
        panel(spec, t, t + PH * SS)

    ny = (PT + len(panels) * (PH + PG) + 46) * SS
    for s in footer:
        d.text((Lm, ny), "- " + s, font=F_NOTE, fill=(70, 72, 78)); ny += 26 * SS

    img = img.resize((W // SS, H // SS), Image.LANCZOS)
    img.save(path)
    print("wrote", path, img.size)

# ---------------------------------------------------------------------------
# build the two charts
# ---------------------------------------------------------------------------
def male_panels():
    m = SEX["male"]; P = PAL_MALE
    return [
        dict(title="Hips VERTICAL (bob)  -  hips Z, m (+up); double bounce",
             curves=[(bob_m, P[0], "", 0, True)], vmin=-0.03, vmax=0.03, ystep=0.01, unit="m",
             fill=(245, 230, 228)),
        dict(title="Hips LATERAL (sway)  -  hips Y, m (+left)",
             curves=[(sway_m, P[1], "", 0, True)], vmin=-0.025, vmax=0.025, ystep=0.01, unit="m"),
        dict(title="Pelvis ROLL (obliquity, rot X)  -  deg (+L hip down)  [7.4 range]",
             curves=[(obliquity(m["obliq"]), P[2], "", 0, True)], vmin=-8, vmax=8, ystep=4, unit="°"),
        dict(title="Pelvis YAW (twist, rot Z)  -  deg (+L hip fwd)  [+-5]",
             curves=[(yaw(m["yaw"]), P[3], "", 0, True)], vmin=-10, vmax=10, ystep=5, unit="°"),
        dict(title="THIGH pitch (hip flex/ext)  -  deg (+back)",
             curves=[(thigh_L, P[5], "", 0, True)],
             vmin=-32, vmax=20, ystep=10, unit="°"),
        dict(title="KNEE flexion  -  deg (+bend)",
             curves=[(knee_L, P[6], "", 0, True)],
             vmin=0, vmax=70, ystep=20, unit="°"),
        dict(title="FOOT pitch  -  deg (+toes down)",
             curves=[(foot_L, P[7], "", 0, True)],
             vmin=-32, vmax=32, ystep=20, unit="°"),
        dict(title="Hip AD/ABDUCTION (frontal)  -  deg (+wider, adducts at midstance)",
             curves=[(hip_frontal(m["abd"]), P[8], "", 0, True)], vmin=-6, vmax=12, ystep=6, unit="°"),
        dict(title="ARM swing  -  deg (+fwd)  [+-18]",
             curves=[(arm_L(m["arm"]), P[9], "", 0, True)],
             vmin=-20, vmax=32, ystep=10, unit="°"),
        dict(title="ELBOW flexion  -  deg (+bend, max when arm fwd)",
             curves=[(elbow_L(m["elb_base"], m["elb_amp"]), P[10], "", 0, True)],
             vmin=8, vmax=28, ystep=4, unit="°"),
        dict(title="STATIC POSES  -  set once on the model, not keyed (deg)",
             rows=[("Anterior pelvic tilt  (spine05 lean, +fwd)", f"{m['tilt']:.1f}°"),
                   ("Forearm carrying angle  (cubitus valgus)",   f"~{m['carry']:.0f}°"),
                   ("Knee adduction  (genu valgum, knees-in)",    f"~{m['knee_val']:.0f}°")]),
    ]

def delta_panels():
    m, f = SEX["male"], SEX["female"]
    return [
        dict(title=f"Pelvis ROLL (obliquity, rot X) (+L hip down)  -  {m['obliq']:.1f}° -> {f['obliq']:.1f}°",
             curves=[(obliquity(m["obliq"]), C_MALE, "male", 0, True),
                     (obliquity(f["obliq"]), C_FEM, "female", 0, True)],
             vmin=-8, vmax=8, ystep=4, unit="°"),
        dict(title=f"Pelvis YAW (twist, rot Z) (+L hip fwd)  -  +-{m['yaw']:.0f}° -> +-{f['yaw']:.0f}°",
             curves=[(yaw(m["yaw"]), C_MALE, "male", 0, True),
                     (yaw(f["yaw"]), C_FEM, "female", 0, True)],
             vmin=-10, vmax=10, ystep=5, unit="°"),
        dict(title=f"Hip AD/ABDUCTION (frontal) (+wider)  -  base {m['abd']:.0f}° -> {f['abd']:.0f}°",
             curves=[(hip_frontal(m["abd"]), C_MALE, "male", 0, True),
                     (hip_frontal(f["abd"]), C_FEM, "female", 0, True)],
             vmin=-6, vmax=12, ystep=6, unit="°"),
        dict(title=f"ARM swing (+fwd)  -  +-{m['arm']:.0f}° -> +-{f['arm']:.0f}°",
             curves=[(arm_R(m["arm"]), C_MALE, "male", 0, True),
                     (arm_R(f["arm"]), C_FEM, "female", 0, True)],
             vmin=-20, vmax=44, ystep=10, unit="°"),   # extra top room so t=0/2/18 split above+below
        dict(title=f"ELBOW flexion (+bend)  -  {m['elb_base']:.0f}+-{m['elb_amp']:.0f}° -> {f['elb_base']:.0f}+-{f['elb_amp']:.0f}°",
             curves=[(elbow_L(m["elb_base"], m["elb_amp"]), C_MALE, "male", 0, True),
                     (elbow_L(f["elb_base"], f["elb_amp"]), C_FEM, "female", 0, True)],
             vmin=8, vmax=40, ystep=8, unit="°"),
        dict(title="STATIC POSES  -  set once, male -> female (deg)",
             rows=[("Anterior pelvic tilt  (spine05 lean)",     f"{m['tilt']:.1f}° -> {f['tilt']:.1f}°"),
                   ("Forearm carrying angle  (cubitus valgus)", f"~{m['carry']:.0f}° -> ~{f['carry']:.0f}°"),
                   ("Knee adduction  (genu valgum, knees-in)",  f"~{m['knee_val']:.0f}° -> ~{f['knee_val']:.0f}°")]),
    ]

render("doc/walk-cycle-male.png",
       "floormat NPC walk - MALE - 20-frame cycle",
       "C contact  D down  P passing  U up.   curves are the body's left side.",
       male_panels(),
       ["Rig axes: up +Z, forward -X, left -Y.   yaw=rot Z, roll=rot X on pelvis.L/.R.   bob=loc Z, sway=loc Y on root.",
        "+ direction per title. pelvis YAW + ROLL confirmed in Blender. hip-frontal + elbow signs still anatomical guesses.",
        "Anterior tilt is a STATIC spine05 lean (no clean pelvis-tilt bone) - see the static-poses box at bottom.",
        "Curves are the body's LEFT side. The right side is identical, shifted +10 frames (half a cycle)."])

render("doc/walk-cycle-female-delta.png",
       "floormat NPC walk - FEMALE DELTAS - change only these axes",
       "male (slate) -> female (magenta).   Everything not shown here is identical to the male chart.",
       delta_panels(),
       ["Only the pelvis (3 axes), hip ad/abduction, arm swing, and elbow change male->female. Bones (MakeHuman f01):",
        "  obliquity+yaw on pelvis.L/.R, ad/abduction on upperleg01, swing on upperarm01, elbow on lowerarm01.",
        "  Anterior tilt has NO clean bone - legs and spine are separate children of root, so spine05 leans the whole torso. Static posture only.",
        "Limb joint rotation is on the 01 segment (upperleg01 etc.); the 02 segment is twist/follow, not keyed. .L/.R per side.",
        "THIGH-PITCH / KNEE / FOOT and the whole step TIMING are unchanged - do not re-key them.",
        "Numbers from Kerrigan 2002 (obliquity 7.4->9.4) + whole-body kinematics (rotation/arm swing)."])

"""
Generate a 20-frame walk-cycle timing chart (PNG) + per-frame value table for the
floormat NPC walk.

Channels (frame 0..20, frame 20 == frame 0, the loop):
  - Hips vertical "bob"  : double bounce, low at DOWN, high at PASSING   [normalized]
  - Hips lateral "sway"  : one weight-shift per cycle, over the stance leg [normalized]
  - Pelvis yaw           : peaks at CONTACT; shoulders counter-rotate     [normalized]
  - Thigh pitch L/R      : hip flex/ext, + forward; THE leg fore-aft swing  [deg]
  - Knee flexion L/R     : biphasic, +18 stance bump, +60 swing peak        [deg]
  - Foot pitch L/R       : +toes-up strike, flat hold, -toes-down push      [deg]
  - Arm swing            : opposes same-side leg, hand drags ~1f            [normalized]
  - Elbow flexion        : forearm bend, more flexed on the forward swing   [deg]

Key poses (Williams 24f re-timed to 20f, down pulled 1f early for weight):
Contact 0/10, Down 2/12, Passing 5/15, Up 8/18.  orange = LEFT, purple = RIGHT.
R-leg curves = the L-leg curve shifted +10 frames.
"""

import math
from PIL import Image, ImageDraw, ImageFont

SS = 2                                  # supersample, downscaled for AA
W, H = 1500 * SS, 2000 * SS
L, R, T, B = 118 * SS, 60 * SS, 96 * SS, 54 * SS
FMAX = 20

BG      = (250, 250, 248)
INK     = (30, 32, 36)
GRID    = (224, 224, 220)
KEYLINE = (148, 152, 160)
C_BOB   = (208, 70, 60)
C_SWAY  = (44, 122, 196)
C_YAW   = (40, 158, 110)
C_GREY  = (150, 150, 156)
C_LEFT  = (224, 132, 44)                # left leg/foot (matches L stance bar)
C_RIGHT = (120, 96, 188)               # right leg/foot (matches R stance bar)
C_ARM_R = C_RIGHT                       # right arm/elbow = purple, matching the right leg
C_HAND_R = (168, 148, 224)             # R hand: lighter purple, dashed (drags behind R arm)
C_ARM_L = C_LEFT                        # left arm/elbow = orange, matching the left leg

def font(sz, bold=False):
    name = "arialbd.ttf" if bold else "arial.ttf"
    try:
        return ImageFont.truetype("C:/Windows/Fonts/" + name, sz * SS)
    except OSError:
        return ImageFont.load_default()

F_TITLE = font(30, True)
F_PANEL = font(17, True)
F_SMALL = font(13)
F_KEY   = font(15, True)
F_VAL   = font(12, True)
F_NOTE  = font(13)

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

def x_of(f):
    return L + (f / FMAX) * (W - L - R)

KEYS = [(0, "C"), (2, "D"), (5, "P"), (8, "U"),
        (10, "C"), (12, "D"), (15, "P"), (18, "U"), (20, "C")]
KEYF = [k for k, _ in KEYS]

# ---- periodic hermite through (frame, value); tangents flattened at extrema ----
def periodic_hermite(cps, period=20.0):
    pts = [(float(t), float(v)) for t, v in cps]
    if pts[-1][0] % period == pts[0][0] % period:
        pts = pts[:-1]
    n = len(pts)

    def tangent(i):
        t0, v0 = pts[i]
        tm, vm = pts[(i - 1) % n]
        tp, vp = pts[(i + 1) % n]
        if tm >= t0:
            tm -= period
        if tp <= t0:
            tp += period
        if (vp - v0) * (v0 - vm) <= 0:
            return 0.0
        return (vp - vm) / (tp - tm)

    def fn(f):
        f = f % period
        for i in range(n):
            t0, v0 = pts[i]
            t1, v1 = pts[(i + 1) % n]
            if t1 <= t0:
                t1 += period
            ff = f if f >= t0 else f + period
            if t0 <= ff <= t1:
                m0, m1 = tangent(i), tangent((i + 1) % n)
                h = t1 - t0
                s = (ff - t0) / h
                return ((2*s**3 - 3*s**2 + 1) * v0 + (s**3 - 2*s**2 + s) * h * m0
                        + (-2*s**3 + 3*s**2) * v1 + (s**3 - s**2) * h * m1)
        return pts[0][1]
    return fn

bob = periodic_hermite([(0, 0.45), (2, 0.0), (5, 1.0), (8, 0.62),
                        (10, 0.45), (12, 0.0), (15, 1.0), (18, 0.62)])
def sway(f):
    return 0.5 + 0.5 * math.sin(2*math.pi * f / 20)
def yaw(f):
    return 0.5 + 0.5 * math.cos(2*math.pi * f / 20)
thigh_L = periodic_hermite([(0, 27), (4.5, 5), (10.5, -13), (15, 8)])
def thigh_R(f):
    return thigh_L(f - 10)
knee_L = periodic_hermite([(0, 5), (2, 18), (6, 5), (10, 10),
                           (12, 40), (14.5, 63), (17, 25), (20, 5)])
def knee_R(f):
    return knee_L(f - 10)
foot_L = periodic_hermite([(0, 25), (2, 0), (4, 0), (6, 0), (8, -8),
                           (10, -18), (12, -24), (13, -12), (14, 0),
                           (16, 8), (18, 15), (20, 25)])
def foot_R(f):
    return foot_L(f - 10)
def arm_R(f):
    return math.cos(2*math.pi * f / 20)
def arm_L(f):
    return -arm_R(f)
def hand_R(f):
    return 1.06 * math.cos(2*math.pi * (f - 1.2) / 20)

# physical-unit wrappers (bob/sway are TRANSLATIONS -> m; yaw/arm are ROTATIONS -> deg)
# m values are for a ~1.7 m rig; they scale with character height, and at sprite
# resolution you will tune to a couple of pixels regardless.
def bob_m(f):                           # vertical hip translation, + up, 0 = contact height
    return (bob(f) - 0.45) * 0.05       # ~0.05 m (5 cm) peak-to-peak
def sway_m(f):                          # lateral hip translation, + toward L (stance at f5)
    return (sway(f) - 0.5) * 0.04       # ~0.04 m (4 cm) peak-to-peak
def yaw_deg(f):                         # pelvis transverse rotation, + = L hip forward
    return 5.0 * math.cos(2*math.pi * f / 20)
def sho_deg(f):                         # shoulders counter-rotate, slightly larger
    return -6.0 * math.cos(2*math.pi * f / 20)
def armR_deg(f):                        # shoulder flex/ext, +25 fwd / -15 back (asymmetric)
    return 5.0 + 20.0 * math.cos(2*math.pi * f / 20)
def armL_deg(f):
    return 5.0 - 20.0 * math.cos(2*math.pi * f / 20)
def handR_deg(f):                       # hand drags ~1.2 f behind the arm
    return 5.0 + 21.0 * math.cos(2*math.pi * (f - 1.2) / 20)
def elbowR_deg(f):                      # forearm flex (0=straight), more bent on the forward swing
    return 22.0 + 10.0 * math.cos(2*math.pi * (f - 1.5) / 20)
def elbowL_deg(f):
    return elbowR_deg(f - 10)

# ---------------- title ----------------
d.text((L, 30*SS), "floormat NPC walk — 20-frame cycle timing + values", font=F_TITLE, fill=INK)
d.text((L, 70*SS),
       "x = frame (0..20, loops); render frames 0..19.   C contact  D down  P passing  U up.   "
       "orange = LEFT, purple = RIGHT (legs + arms).   R leg curve = L leg curve shifted +10f.",
       font=F_SMALL, fill=(96, 98, 104))

def vgrid(y0, y1):
    for f in range(0, FMAX + 1):
        x = x_of(f)
        is_key = f in KEYF
        d.line([(x, y0), (x, y1)], fill=KEYLINE if is_key else GRID,
               width=2*SS if is_key else 1*SS)

def draw_curve(pts, color, width, dash=0):
    if dash:
        for i in range(len(pts) - 1):
            if (i // dash) % 2 == 0:
                d.line([pts[i], pts[i+1]], fill=color, width=width)
    else:
        d.line(pts, fill=color, width=width, joint="curve")

def panel(title, ytop, ybot, curves, vmin, vmax, fill_first=None, ystep=None, unit=""):
    d.rectangle([L, ytop, W - R, ybot], outline=GRID, width=1*SS)
    vgrid(ytop, ybot)
    pad = 16 * SS

    def y_of(v):
        return ybot - pad - (v - vmin) / (vmax - vmin) * (ybot - ytop - 2*pad)

    # horizontal value grid + left-axis numeric labels
    if ystep:
        # integer-indexed so fractional ystep (e.g. 0.01 m) doesn't drift the
        # gridlines or print labels like 1e-17m from accumulated float error
        for nstep in range(math.ceil(vmin / ystep - 1e-9),
                            math.floor(vmax / ystep + 1e-9) + 1):
            v = nstep * ystep
            yy = y_of(v)
            is_zero = abs(v) < 1e-6
            d.line([(L, yy), (W - R, yy)],
                   fill=(198, 200, 196) if is_zero else (234, 234, 230),
                   width=2*SS if is_zero else 1*SS)
            lab = f"{v:g}{unit}"
            tw = d.textlength(lab, font=F_SMALL)
            d.text((L - 10*SS - tw, yy - 8*SS), lab, font=F_SMALL,
                   fill=INK if is_zero else (148, 150, 156))

    if unit == "m":
        valfmt = lambda v: f"{v:+.3f}"
    elif unit:
        valfmt = lambda v: f"{v:.0f}{unit}"
    else:
        valfmt = lambda v: f"{v:.2f}"

    for ci, (fn, color, label, dash, dots) in enumerate(curves):
        pts = []
        f = 0.0
        while f <= FMAX + 1e-6:
            pts.append((x_of(f), y_of(fn(f))))
            f += 0.1
        if ci == 0 and fill_first:
            d.polygon(pts + [(x_of(FMAX), ybot), (x_of(0), ybot)], fill=fill_first)
        draw_curve(pts, color, 3*SS if not dash else 2*SS, dash)
        if dots:
            for k in KEYF:
                if k == 20:
                    continue
                x, y = x_of(k), y_of(fn(k))
                d.ellipse([x-5*SS, y-5*SS, x+5*SS, y+5*SS],
                          fill=color, outline=BG, width=1*SS)
                # exact value label, placed above the dot (below if near the top)
                s = valfmt(fn(k))
                tw = d.textlength(s, font=F_VAL)
                ty = y - 22*SS if (y - 24*SS) > ytop else y + 10*SS
                d.rectangle([x - tw/2 - 3*SS, ty - 2*SS, x + tw/2 + 3*SS, ty + 16*SS],
                            fill=(255, 255, 255))
                d.text((x - tw/2, ty), s, font=F_VAL, fill=color)

    d.text((L + 8*SS, ytop + 6*SS), title, font=F_PANEL, fill=INK)
    lx = W - R - 12*SS
    for fn, color, label, dash, dots in reversed(curves):
        if not label:
            continue
        tw = d.textlength(label, font=F_SMALL)
        lx -= tw
        d.text((lx, ytop + 8*SS), label, font=F_SMALL, fill=color)
        lx -= 36*SS
        yl = ytop + 16*SS
        x0s, x1s = lx + 4*SS, lx + 28*SS
        if dash:                        # dashed swatch mirrors the dashed curve
            n = 7
            step = (x1s - x0s) / n
            for k in range(0, n, 2):
                xa = x0s + k * step
                d.line([(xa, yl), (min(xa + step, x1s), yl)], fill=color, width=3*SS)
        else:
            d.line([(x0s, yl), (x1s, yl)], fill=color, width=3*SS)
        lx -= 16*SS

# ---------------- key-pose label row + stance strip ----------------
def key_row(yc):                        # tick + pose letter above + frame number below
    for k, lab in KEYS:
        x = x_of(k)
        d.line([(x, yc), (x, yc + 12*SS)], fill=KEYLINE, width=2*SS)
        d.text((x - 5*SS, yc - 18*SS), lab, font=F_KEY, fill=INK)
        d.text((x - 6*SS, yc + 14*SS), str(k), font=F_SMALL, fill=(120, 122, 128))

key_row(104 * SS)

st_y = 148 * SS
bar_h = 15 * SS
def stance_bar(y, intervals, color, label):
    d.text((L - 108*SS, y - 1*SS), label, font=F_SMALL, fill=INK)
    for a, b in intervals:
        d.rectangle([x_of(a), y, x_of(b), y + bar_h], fill=color)
stance_bar(st_y, [(0, 12)], C_LEFT, "L foot down")
stance_bar(st_y + bar_h + 4*SS, [(10, 20), (0, 2)], C_RIGHT, "R foot down")
d.text((L + 8*SS, st_y - 20*SS), "foot contact (planted = no sliding)", font=F_PANEL, fill=INK)

# ---------------- panels ----------------
PT, PH, PG = 214, 168, 22
def py(i):
    t = (PT + i * (PH + PG)) * SS
    return t, t + PH * SS

t, b = py(0)
panel("Hips VERTICAL (bob)  —  TRANSLATION, m (+up, 0=contact); double bounce, low D high P", t, b,
      [(bob_m, C_BOB, "", 0, True)], -0.03, 0.03, fill_first=(245, 230, 228), ystep=0.01, unit="m")
t, b = py(1)
panel("Hips LATERAL (sway)  —  TRANSLATION, m (+ toward L); one weight-shift per cycle", t, b,
      [(sway_m, C_SWAY, "", 0, True)], -0.025, 0.025, ystep=0.01, unit="m")
t, b = py(2)
panel("Pelvis YAW (twist)  —  rotation deg (+ L hip fwd); peaks at contact; shoulders counter (dashed)", t, b,
      [(yaw_deg, C_YAW, "pelvis", 0, True),
       (sho_deg, C_GREY, "shoulders", 4, False)], -8.0, 8.0, ystep=4, unit="°")
t, b = py(3)
panel("THIGH pitch = hip flex/ext  —  THE leg swing: fwd at C, vertical at P, back at toe-off",
      t, b, [(thigh_L, C_LEFT, "L thigh", 0, True),
             (thigh_R, C_RIGHT, "R thigh", 0, False)], -20.0, 32.0, ystep=10, unit="°")
t, b = py(4)
panel("KNEE flexion (0 = straight)  —  small bump at D, big swing peak after toe-off",
      t, b, [(knee_L, C_LEFT, "L knee", 0, True),
             (knee_R, C_RIGHT, "R knee", 0, False)], 0.0, 70.0, ystep=20, unit="°")
t, b = py(5)
panel("FOOT pitch (+ toes-up)  —  strike +25, slap flat, hold, heel peel at U, push -24, lift in swing",
      t, b, [(foot_L, C_LEFT, "L foot", 0, True),
             (foot_R, C_RIGHT, "R foot", 0, False)], -32.0, 32.0, ystep=20, unit="°")
t, b = py(6)
panel("ARM swing  —  rotation deg (+fwd, +25/-15 asymmetric); opposes same-side leg; hand drags (dashed)",
      t, b, [(armR_deg, C_ARM_R, "R arm", 0, True),
             (armL_deg, C_ARM_L, "L arm", 0, False),
             (handR_deg, C_HAND_R, "R hand", 4, False)], -20.0, 32.0, ystep=10, unit="°")
t, b = py(7)
panel("ELBOW flexion (0 = straight)  —  forearm bend; more flexed on the forward swing, straighter swinging back",
      t, b, [(elbowR_deg, C_ARM_R, "R elbow", 0, True),
             (elbowL_deg, C_ARM_L, "L elbow", 0, False)], 0.0, 40.0, ystep=10, unit="°")

# ---------------- bottom frame guide (mirror of the top key-pose row) ----------------
gy = py(7)[1] + 40 * SS
key_row(gy)

# ---------------- footer ----------------
notes = [
    "THIGH (hip) is the actual step — knee and foot ride on top of it. Fwd at C, near-vertical at P, max back at toe-off (~f11).",
    "Knee is never dead straight: ~5° at C, ~18° bump at D (absorbs the hit), ~60° peak just after toe-off for clearance.",
    "Foot: toes-up at strike, slap flat in ~2f, hold DEAD FLAT while planted, heel peels at U, max toes-down after opposite contact.",
    "Thigh/knee/foot are per-LEG curves; R = L shifted +10f (orange=L, purple=R). The bob is their net effect at 2x frequency.",
    "Arms oppose the same-side leg; elbow bends more swinging forward; let the hand drag ~1 frame (overlap).",
    "Signs are anatomical — verify + direction per bone in Blender; flip the channel if local axis is reversed.",
]
ny = gy + 46 * SS
for s in notes:
    d.text((L, ny), "• " + s, font=F_NOTE, fill=(70, 72, 78))
    ny += 22 * SS

img = img.resize((W // SS, H // SS), Image.LANCZOS)
img.save("doc/walk-cycle-20f.png")
print("wrote doc/walk-cycle-20f.png", img.size)

# ---------------- per-frame value table (stdout) ----------------
names = ["Thigh", "Knee", "Foot", "Yaw", "ArmR", "ElbowR", "BobM", "SwayM"]
fns   = [thigh_L, knee_L, foot_L, yaw_deg, armR_deg, elbowR_deg, bob_m, sway_m]
meters = {"BobM", "SwayM"}
posemap = {k: lab for k, lab in KEYS}
print("\nPer-frame values (LEFT leg; RIGHT = same at frame-10).")
print("Thigh/Knee/Foot/Yaw/ArmR/ElbowR in degrees; BobM/SwayM in metres (~1.7 m rig).")
print("frame\tPose\t" + "\t".join(names))
for f in range(0, 21):
    row = [str(f), posemap.get(f, "")]
    for nm, fn in zip(names, fns):
        v = fn(f)
        row.append(f"{v:+.3f}" if nm in meters else f"{v:.0f}")
    print("\t".join(row))

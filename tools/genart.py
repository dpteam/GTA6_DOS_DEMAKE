"""Sprites -> build/gen/assets.c + assets.h.

Everything is drawn here, at 1x, in indexed colour:

  * humans   -- 4 directions, recoloured into six characters (player, Lucia,
                three pedestrians, a cop). Top-down-ish: you see the hair from
                above and a bit of face, like GTA 1 on a 386.
  * cars     -- built as a rounded body + windows + wheels, then rotated to
                16 headings with nearest-neighbour sampling. Rotating a
                palette-indexed canvas is fine as long as nobody interpolates.
  * props    -- palms, bins, barrels, streetlights.
  * pickups  -- money, briefcase, health, armour, ammo, star.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pal  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")
P = pal.PAL

# ------------------------------------------------------------------ humans ---
# 12x16. '.' is transparent. Legend below is deliberately small: at this size
# a character is six pixels of hair and a jacket, and that is enough.
LEGEND = {
    "h": P["wood2"], "H": P["wood3"],
    "s": P["skin4"], "S": P["skin6"], "e": P["void1"], "m": P["skin1"],
    "j": P["teal4"], "J": P["teal6"], "k": P["teal2"],
    "p": P["navy4"], "P": P["navy5"], "b": P["void2"], "w": P["white6"],
}

DOWN = [
    "....hhhh....",
    "...hhhhhh...",
    "..hhsssshh..",
    "..hssssssh..",
    "...sesses...",
    "...ssmmss...",
    "...jjjjjj...",
    "..jjjjjjjj..",
    ".jjjjJJJJjj.",
    ".jjjjjjjjjj.",
    ".sjjjjjjjjs.",
    "..pppppppp..",
    "..pppppppp..",
    "..ppp..ppp..",
    "..ppp..ppp..",
    ".bbbb..bbbb.",
]

UP = [
    "....hhhh....",
    "...hhhhhh...",
    "..hhhhhhhh..",
    "..hhhhhhhh..",
    "..hhhhhhhh..",
    "...hhhhhh...",
    "...jjjjjj...",
    "..jjjjjjjj..",
    ".jjjjJJJJjj.",
    ".jjjjjjjjjj.",
    ".sjjjjjjjjs.",
    "..pppppppp..",
    "..pppppppp..",
    "..ppp..ppp..",
    "..ppp..ppp..",
    ".bbbb..bbbb.",
]

LEFT = [
    "....hhhh....",
    "...hhhhhh...",
    "..hsssshhh..",
    "..sssssshh..",
    "..esssss....",
    "..smmsss....",
    "...jjjjjj...",
    "..jjjjjjjj..",
    "..jjjJJJjj..",
    "..jjjjjjjj..",
    ".sjjjjjjjj..",
    "..pppppppp..",
    "..pppppppp..",
    "..ppppppp...",
    ".pppppppp...",
    ".bbb..bbbb..",
]

# per-character recolour: ramp substitutions applied to the base art
HUMANS = [
    # name,            jacket,      pants,       hair
    ("player",         "teal",      "navy",      "wood"),
    ("lucia",          "neonpink",  "void",      "void"),
    ("ped_a",          "orange",    "asphalt",   "gold"),
    ("ped_b",          "white",     "brick",     "wood"),
    ("ped_c",          "purple",    "darkgreen", "void"),
    ("cop",            "navy",      "navy",      "void"),
]

BASE_RAMPS = {"j": "teal", "J": "teal", "k": "teal",
              "p": "navy", "P": "navy", "h": "wood", "H": "wood"}


def remap(idx, sub, dark):
    if idx == 0:
        return 0
    info = pal.RAMP_INFO.get(idx)
    if not info:
        return idx
    name, i, n = info
    new = sub.get(name)
    if not new:
        return idx
    if dark:
        i = max(0, i - 2)
    return P["%s%d" % (new, min(n - 1, i))]


def human(rows, sub, dark_hair=False):
    c = pal.Canvas(12, 16)
    c.from_ascii(rows, LEGEND)
    out = pal.Canvas(12, 16)
    for i, v in enumerate(c.px):
        if not v:
            continue
        info = pal.RAMP_INFO.get(v)
        if not info:
            out.px[i] = v
            continue
        name, step, n = info
        target = sub.get(name)
        if target is None:
            out.px[i] = v
            continue
        if name in ("wood", "void") and dark_hair:
            step = 0
        out.px[i] = P["%s%d" % (target, min(n - 1, step))]
    return out


def human_set(rows, sub):
    d = human(rows[0], sub)
    u = human(rows[1], sub)
    l = human(rows[2], sub)
    r = l.flip_h()
    return [d, u, l, r]


# -------------------------------------------------------------------- cars ---
def rounded(w, h, r):
    """Pixel mask of a rounded rectangle."""
    m = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            dx = min(x, w - 1 - x)
            dy = min(y, h - 1 - y)
            if dx >= r or dy >= r:
                m[y][x] = True
            elif (r - dx) ** 2 + (r - dy) ** 2 <= r * r + r:
                m[y][x] = True
    return m


def car(body="red", w=28, h=15, kind="sedan"):
    c = pal.Canvas(w, h)
    m = rounded(w, h, 3)
    body_lo, body_hi = 0.34, 0.62
    for y in range(h):
        for x in range(w):
            if not m[y][x]:
                continue
            # light from the north-west, plus a 1px outline
            edge = not (m[y][min(w - 1, x + 1)] and m[y][max(0, x - 1)] and
                        m[min(h - 1, y + 1)][x] and m[max(0, y - 1)][x])
            shade = body_lo + (1.0 - y / (h - 1.0)) * 0.18
            if x > w * 0.78:
                shade += 0.10            # nose catches the light
            if edge:
                c.set(x, y, pal.ramp(body, 8, 0.10))
            else:
                c.set(x, y, pal.ramp(body, 8, shade))
    # windows: windshield near the nose, rear window near the tail, and a
    # painted roof between them. The roof must stay clearly *body* coloured,
    # otherwise the car reads as one long piece of glass with wheels.
    if kind != "van":
        def pane(x0, x1, base):
            for y in range(3, h - 3):
                for x in range(x0, x1):
                    if not m[y][x]:
                        continue
                    # bright top edge = reflection of the sky
                    v = base + (0.22 if y == 3 else 0.0) + (y % 3) * 0.04
                    c.set(x, y, pal.ramp("glass", 8, v))
        pane(int(w * 0.62), int(w * 0.84), 0.42)   # windshield
        pane(int(w * 0.12), int(w * 0.28), 0.34)   # rear window
        for y in range(4, h - 4):
            for x in range(int(w * 0.31), int(w * 0.59)):
                if m[y][x]:
                    c.set(x, y, pal.ramp(body, 8, 0.74))
        # a thin body-coloured pillar either side of the roof
        for y in range(3, h - 3):
            for x in (int(w * 0.29), int(w * 0.30), int(w * 0.59), int(w * 0.60)):
                if m[y][x]:
                    c.set(x, y, pal.ramp(body, 8, 0.44))
    else:
        for y in range(2, h - 2):
            for x in range(int(w * 0.55), int(w * 0.80)):
                if m[y][x]:
                    c.set(x, y, pal.ramp("glass", 8, 0.50))
        for y in range(4, h - 4):
            for x in range(int(w * 0.30), int(w * 0.52)):
                if m[y][x]:
                    c.set(x, y, pal.ramp(body, 8, 0.74))
    # wheels poke out of the top and bottom edges
    for (wx, wy) in ((2, 0), (w - 7, 0), (2, h - 2), (w - 7, h - 2)):
        for dy in range(2):
            for dx in range(4):
                if 0 <= wx + dx < w and 0 <= wy + dy < h:
                    c.set(wx + dx, wy + dy, pal.ramp("void", 8, 0.62))
        c.set(wx, wy, pal.ramp("void", 8, 0.30))
        c.set(wx + 3, wy + 1, pal.ramp("void", 8, 0.30))
    # lights
    for y in (3, h - 4):
        c.set(w - 2, y, P["yellow6"])
        c.set(w - 3, y, P["yellow4"])
        c.set(1, y, P["red6"])
    if kind == "taxi":
        for x in range(0, w, 4):
            c.set(x, 7, P["white7"])
            c.set(x + 1, 7, P["void1"])
        c.rect(w // 2 - 3, 1, 6, 2, P["yellow7"])
    if kind == "cop":
        c.rect(w // 2 - 4, h // 2 - 2, 3, 4, P["blue6"])
        c.rect(w // 2 - 1, h // 2 - 2, 3, 4, P["red6"])
        for y in range(3, h - 3):
            c.set(int(w * 0.42), y, P["white7"])
            c.set(int(w * 0.44), y, P["white5"])
    if kind == "sports":
        for y in range(2, h - 2):
            c.set(int(w * 0.34), y, pal.ramp("neoncyan", 8, 0.75))
    return c


CARS = [
    # name,        body ramp, w,  h,  kind
    ("sedan_red",   "red",     28, 15, "sedan"),
    ("sedan_blue",  "blue",    28, 15, "sedan"),
    ("sedan_white", "white",   28, 15, "sedan"),
    ("sports_neon", "neonpink", 30, 14, "sports"),
    ("taxi",        "yellow",  28, 15, "taxi"),
    ("van",         "concrete", 32, 17, "van"),
    ("cop",         "navy",    28, 15, "cop"),
]
CAR_DIRS = 16


# ------------------------------------------------------------------- props ---
def palm(seed=0, size=26):
    c = pal.Canvas(size, size)
    cx = cy = size // 2
    c.disc(cx, cy, 3, pal.ramp("wood", 8, 0.45))
    c.disc(cx, cy, 2, pal.ramp("wood", 8, 0.30))
    import math
    for k in range(9):
        a = (k / 9.0) * math.tau + seed * 0.35
        ln = size * 0.46 + (k % 3) * 1.4
        for t in range(2, int(ln)):
            x = int(cx + math.cos(a) * t)
            y = int(cy + math.sin(a) * t * 0.85)
            v = 0.78 - (t / ln) * 0.45
            c.set(x, y, pal.ramp("palm", 8, v))
            if t % 4 == 1:
                c.set(x + 1, y + 1, pal.ramp("palm", 8, v - 0.18))
                c.set(x, y - 1, pal.ramp("palm", 8, v + 0.12))
    c.set(cx, cy, pal.ramp("palm", 8, 0.95))
    return c


def bush(seed=0):
    c = pal.Canvas(14, 12)
    c.ellipse(7, 6, 6, 5, pal.ramp("darkgreen", 8, 0.55))
    c.ellipse(6, 5, 4, 3, pal.ramp("green", 10, 0.60))
    for i in range(10):
        x = (i * 5 + seed * 3) % 12 + 1
        y = (i * 7 + seed) % 10 + 1
        c.set(x, y, pal.ramp("green", 10, 0.85))
    return c


def barrel():
    c = pal.Canvas(12, 12)
    c.disc(6, 6, 5, pal.ramp("red", 10, 0.45))
    c.disc(6, 6, 3, pal.ramp("red", 10, 0.62))
    c.disc(6, 6, 1, pal.ramp("metal", 8, 0.55))
    for a in range(0, 360, 45):
        import math
        x = int(6 + math.cos(math.radians(a)) * 4)
        y = int(6 + math.sin(math.radians(a)) * 4)
        c.set(x, y, pal.ramp("metal", 8, 0.75))
    return c


def dumpster():
    c = pal.Canvas(18, 12)
    c.rect(1, 2, 16, 9, pal.ramp("darkgreen", 8, 0.55))
    c.rect(1, 2, 16, 2, pal.ramp("darkgreen", 8, 0.75))
    c.rect(2, 5, 14, 1, pal.ramp("darkgreen", 8, 0.32))
    c.rect(3, 8, 12, 1, pal.ramp("darkgreen", 8, 0.32))
    c.rect(0, 10, 17, 1, pal.ramp("void", 8, 0.4))
    return c


def streetlight():
    c = pal.Canvas(12, 12)
    c.disc(6, 6, 5, pal.ramp("metal", 8, 0.28))
    c.disc(6, 6, 4, pal.ramp("metal", 8, 0.48))
    c.disc(6, 6, 2, pal.ramp("yellow", 8, 0.95))
    c.disc(6, 6, 1, pal.ramp("yellow", 8, 0.75))
    return c


def billboard():
    c = pal.Canvas(20, 14)
    c.rect(1, 1, 18, 10, pal.ramp("void", 8, 0.45))
    c.rect(2, 2, 16, 8, pal.ramp("neonpink", 8, 0.60))
    c.rect(3, 3, 14, 2, pal.ramp("neoncyan", 8, 0.80))
    c.rect(3, 6, 9, 1, pal.ramp("white", 8, 0.80))
    c.rect(3, 8, 12, 1, pal.ramp("white", 8, 0.60))
    c.rect(3, 11, 2, 3, pal.ramp("metal", 8, 0.40))
    c.rect(15, 11, 2, 3, pal.ramp("metal", 8, 0.40))
    return c


# ----------------------------------------------------------------- pickups ---
def moneybag():
    c = pal.Canvas(10, 11)
    c.disc(5, 6, 4, pal.ramp("green", 10, 0.55))
    c.disc(5, 6, 3, pal.ramp("green", 10, 0.70))
    c.rect(3, 1, 4, 2, pal.ramp("wood", 8, 0.55))
    c.rect(3, 3, 4, 1, pal.ramp("gold", 8, 0.85))
    c.set(4, 5, pal.ramp("yellow", 8, 0.95))
    c.set(5, 5, pal.ramp("yellow", 8, 0.95))
    c.set(4, 6, pal.ramp("yellow", 8, 0.85))
    c.set(5, 6, pal.ramp("yellow", 8, 0.85))
    c.set(4, 7, pal.ramp("yellow", 8, 0.75))
    c.set(5, 7, pal.ramp("yellow", 8, 0.75))
    return c


def briefcase():
    c = pal.Canvas(12, 10)
    c.rect(1, 3, 10, 6, pal.ramp("wood", 8, 0.45))
    c.rect(1, 3, 10, 1, pal.ramp("wood", 8, 0.65))
    c.rect(4, 1, 4, 2, pal.ramp("metal", 8, 0.45))
    c.rect(5, 5, 2, 2, pal.ramp("gold", 8, 0.85))
    c.rect(2, 4, 8, 1, pal.ramp("wood", 8, 0.30))
    return c


def health():
    c = pal.Canvas(10, 10)
    c.rect(1, 1, 8, 8, pal.ramp("white", 8, 0.85))
    c.rect(1, 1, 8, 1, pal.ramp("white", 8, 0.95))
    c.rect(4, 2, 2, 6, pal.ramp("red", 10, 0.75))
    c.rect(2, 4, 6, 2, pal.ramp("red", 10, 0.75))
    return c


def armour():
    c = pal.Canvas(10, 10)
    for y in range(1, 9):
        w = 8 - abs(y - 5)
        c.rect(5 - w // 2, y, w, 1, pal.ramp("blue", 8, 0.55))
    c.rect(4, 3, 2, 4, pal.ramp("neoncyan", 8, 0.85))
    c.rect(3, 4, 4, 2, pal.ramp("neoncyan", 8, 0.85))
    return c


def ammo():
    c = pal.Canvas(11, 9)
    c.rect(1, 2, 9, 6, pal.ramp("darkgreen", 8, 0.60))
    c.rect(1, 2, 9, 1, pal.ramp("darkgreen", 8, 0.85))
    c.rect(2, 4, 7, 1, pal.ramp("gold", 8, 0.90))
    c.rect(2, 6, 7, 1, pal.ramp("gold", 8, 0.70))
    return c


def star(filled=True):
    c = pal.Canvas(11, 11)
    import math
    pts = []
    for i in range(10):
        a = math.radians(-90 + i * 36)
        rr = 5.2 if i % 2 == 0 else 2.2
        pts.append((5 + math.cos(a) * rr, 5 + math.sin(a) * rr))
    # scanline fill of a 10-gon, good enough at 11x11
    for y in range(11):
        xs = []
        for i in range(10):
            x0, y0 = pts[i]
            x1, y1 = pts[(i + 1) % 10]
            if (y0 <= y < y1) or (y1 <= y < y0):
                xs.append(x0 + (y - y0) / (y1 - y0) * (x1 - x0))
        xs.sort()
        for i in range(0, len(xs) - 1, 2):
            for x in range(int(xs[i] + 0.5), int(xs[i + 1] + 0.5) + 1):
                c.set(x, y, pal.ramp("gold", 8, 0.9 if filled else 0.30))
    return c


def corpse():
    c = pal.Canvas(16, 9)
    c.ellipse(8, 4, 7, 3, pal.ramp("blood", 6, 0.45))
    c.ellipse(8, 4, 5, 2, pal.ramp("blood", 6, 0.30))
    c.rect(5, 2, 3, 3, pal.ramp("skin", 8, 0.45))
    c.rect(9, 3, 4, 2, pal.ramp("wood", 8, 0.35))
    return c


def blood():
    c = pal.Canvas(10, 8)
    c.ellipse(5, 4, 4, 3, pal.ramp("blood", 6, 0.35))
    c.ellipse(4, 3, 2, 1, pal.ramp("blood", 6, 0.55))
    c.set(8, 6, pal.ramp("blood", 6, 0.45))
    return c


def main():
    os.makedirs(GEN, exist_ok=True)
    sprites = []            # (name, canvas)
    groups = []             # (group name, first index, count)

    # humans: 6 variants x 4 directions
    first = len(sprites)
    for name, jacket, pants, hair in HUMANS:
        sub = dict(BASE_RAMPS)
        sub["teal"] = jacket
        sub["navy"] = pants
        sub["wood"] = hair
        for d, cv in enumerate(human_set([DOWN, UP, LEFT], sub)):
            sprites.append(("human_%s_%d" % (name, d), cv))
    groups.append(("HUMAN", first, len(HUMANS) * 4))

    first = len(sprites)
    for name, ramp, w, h, kind in CARS:
        base = car(ramp, w, h, kind)
        for d in range(CAR_DIRS):
            sprites.append(("car_%s_%d" % (name, d), base.rotate(d * 360.0 / CAR_DIRS)))
    groups.append(("CAR", first, len(CARS) * CAR_DIRS))

    props = [("palm1", palm(0)), ("palm2", palm(3)), ("bush", bush(0)),
             ("bush2", bush(5)), ("barrel", barrel()), ("dumpster", dumpster()),
             ("lamp", streetlight()), ("billboard", billboard())]
    first = len(sprites)
    sprites += props
    groups.append(("PROP", first, len(props)))

    picks = [("bag", moneybag()), ("case", briefcase()), ("health", health()),
             ("armour", armour()), ("ammo", ammo()), ("star", star(True)),
             ("star_empty", star(False))]
    first = len(sprites)
    sprites += picks
    groups.append(("PICK", first, len(picks)))

    decals = [("corpse", corpse()), ("blood", blood())]
    first = len(sprites)
    sprites += decals
    groups.append(("DECAL", first, len(decals)))

    # ---- header
    hdr = ["/* generated by tools/genart.py - do not edit */",
           "#ifndef GTA_ASSETS_H",
           "#define GTA_ASSETS_H",
           "",
           "#define N_SPRITES %d" % len(sprites),
           "#define HUMAN_DIRS 4",
           "#define CAR_DIRS %d" % CAR_DIRS,
           "#define N_HUMAN_TYPES %d" % len(HUMANS),
           "#define N_CAR_MODELS %d" % len(CARS),
           "",
           "enum {"]
    for i, (name, _) in enumerate(sprites):
        hdr.append("  SPR_%s = %d," % (name.upper(), i))
    hdr.append("};")
    hdr.append("")
    for gname, first_i, cnt in groups:
        hdr.append("#define SPR_%s_BASE %d" % (gname, first_i))
        hdr.append("#define SPR_%s_COUNT %d" % (gname, cnt))
    hdr.append("")
    hdr.append("typedef struct { unsigned char w, h; const unsigned char *px; } Sprite;")
    hdr.append("extern const Sprite g_sprites[N_SPRITES];")
    hdr.append("extern const unsigned char g_pal[768];")
    hdr.append("")
    hdr.append("#endif")
    with open(os.path.join(GEN, "assets.h"), "w", encoding="ascii") as f:
        f.write("\n".join(hdr) + "\n")

    with open(os.path.join(GEN, "assets.c"), "w", encoding="ascii") as f:
        f.write("/* generated by tools/genart.py - do not edit */\n")
        f.write("#include \"assets.h\"\n\n")
        for i, (name, cv) in enumerate(sprites):
            f.write("static const unsigned char spr_%d[%d] = {\n" % (i, len(cv.px)))
            for y in range(cv.h):
                row = cv.px[y * cv.w:(y + 1) * cv.w]
                f.write("  " + ",".join("%d" % b for b in row) + ",\n")
            f.write("};\n")
        f.write("\nconst Sprite g_sprites[N_SPRITES] = {\n")
        for i, (name, cv) in enumerate(sprites):
            f.write("  {%d,%d,spr_%d},  /* %s */\n" % (cv.w, cv.h, i, name))
        f.write("};\n")

    total = sum(len(cv.px) for _, cv in sprites)
    print("wrote assets.c/h: %d sprites, %d bytes of pixels" % (len(sprites), total))

    sh = pal.sheet([cv for _, cv in sprites[:24]], scale=3, cols=8)
    if sh:
        sh.save(os.path.join(ROOT, "build", "preview_humans.png"))
    sh = pal.sheet([cv for _, cv in sprites if _.startswith("car_")][:32],
                   scale=3, cols=8)
    if sh:
        sh.save(os.path.join(ROOT, "build", "preview_cars.png"))
    rest = [cv for _, cv in sprites if not _.startswith(("car_", "human_"))]
    sh = pal.sheet(rest, scale=4, cols=8)
    if sh:
        sh.save(os.path.join(ROOT, "build", "preview_props.png"))


if __name__ == "__main__":
    main()

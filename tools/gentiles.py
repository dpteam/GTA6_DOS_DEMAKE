"""Procedural 16x16 city tiles -> build/gen/tiles.c + tiles.h.

Hand-drawing asphalt as ASCII is pointless: it is noise with an opinion.
Every tile here is a small function that stamps a palette ramp plus a
deterministic dither, so two runs give byte-identical output.

Tile ids are the contract with C. `tiles.h` also carries the flags:
solid (blocks movement), roof (gets the pseudo-3D border + shadow),
water (kills you politely).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pal  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")
W = H = 16

# name, flag string
TILES = [
    ("void",        ""),
    ("asphalt",     ""),
    ("asphalt_v",   ""),          # lane dashes running north-south
    ("asphalt_h",   ""),          # lane dashes running east-west
    ("crosswalk",   ""),
    ("manhole",     ""),
    ("sidewalk",    ""),
    ("curb_n",      ""),
    ("curb_s",      ""),
    ("curb_e",      ""),
    ("curb_w",      ""),
    ("grass",       ""),
    ("grass2",      ""),
    ("sand",        ""),
    ("water",       "water"),
    ("water2",      "water"),
    ("parking",     ""),
    ("roof_grey",   "solid roof"),
    ("roof_red",    "solid roof"),
    ("roof_teal",   "solid roof"),
    ("roof_glass",  "solid roof"),
    ("roof_brown",  "solid roof"),
    ("dock",        ""),
    ("plaza",       ""),
    ("dirt",        ""),
    ("alley",       ""),
]


def h32(x, y, seed=0):
    n = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return (n ^ (n >> 16)) & 0xFF


def tile():
    return pal.Canvas(W, H, 0)


def R(name, t):
    return pal.ramp(name, pal.RAMPS[name][1], t)


def noisy(c, rampname, lo, hi, seed):
    n = pal.RAMPS[rampname][1]
    for y in range(H):
        for x in range(W):
            v = lo + (h32(x, y, seed) / 255.0) * (hi - lo)
            c.set(x, y, R(rampname, v))


# ---------------------------------------------------------------- ground -----

def t_asphalt(seed=1):
    c = tile()
    noisy(c, "asphalt", 0.18, 0.55, seed)
    for (px, py) in ((2, 5), (11, 2), (7, 12), (13, 9), (4, 14)):
        c.set(px, py, R("asphalt", 0.72))
        c.set(px + 1, py, R("asphalt", 0.60))
    return c


def t_asphalt_v():
    c = t_asphalt(2)
    for y in range(H):
        if (y // 4) % 2 == 0:
            c.set(7, y, R("yellow", 0.72))
            c.set(8, y, R("yellow", 0.55))
    return c


def t_asphalt_h():
    c = t_asphalt(3)
    for x in range(W):
        if (x // 4) % 2 == 0:
            c.set(x, 7, R("yellow", 0.72))
            c.set(x, 8, R("yellow", 0.55))
    return c


def t_crosswalk():
    c = t_asphalt(4)
    for y in range(1, 15):
        for x in range(1, 15):
            if (x // 3) % 2 == 0:
                c.set(x, y, R("white", 0.86))
    return c


def t_manhole():
    c = t_asphalt(5)
    cx = cy = 8
    c.disc(cx, cy, 5, R("metal", 0.30))
    c.disc(cx, cy, 4, R("metal", 0.48))
    for y in range(3, 13):
        for x in range(3, 13):
            if (x + y) % 3 == 0 and (x - cx) ** 2 + (y - cy) ** 2 < 16:
                c.set(x, y, R("metal", 0.28))
    return c


def t_sidewalk():
    c = tile()
    noisy(c, "concrete", 0.30, 0.52, 11)
    for y in range(H):
        for x in range(H):
            if x % 8 == 0 or y % 8 == 0:
                c.set(x, y, R("concrete", 0.22))
    for x in range(W):
        c.set(x, 0, R("concrete", 0.62))
    return c


def curb(side):
    c = t_sidewalk()
    if side == "n":
        for x in range(W):
            c.set(x, 0, R("concrete", 0.80))
            c.set(x, 1, R("concrete", 0.66))
            c.set(x, 2, R("concrete", 0.40))
    elif side == "s":
        for x in range(W):
            c.set(x, 15, R("concrete", 0.30))
            c.set(x, 14, R("concrete", 0.70))
            c.set(x, 13, R("concrete", 0.82))
    elif side == "e":
        for y in range(H):
            c.set(15, y, R("concrete", 0.30))
            c.set(14, y, R("concrete", 0.70))
            c.set(13, y, R("concrete", 0.82))
    else:
        for y in range(H):
            c.set(0, y, R("concrete", 0.80))
            c.set(1, y, R("concrete", 0.66))
            c.set(2, y, R("concrete", 0.40))
    return c


def t_grass(seed=21, flowers=False):
    c = tile()
    noisy(c, "green", 0.16, 0.44, seed)
    for i in range(26):
        px = h32(i, 3, seed) % W
        py = h32(i, 9, seed) % H
        c.set(px, py, R("green", 0.72))
        c.set(px, (py + 1) % H, R("green", 0.56))
    if flowers:
        for i in range(5):
            px = h32(i, 17, seed) % W
            py = h32(i, 23, seed) % H
            col = R("pink", 0.85) if i % 2 else R("yellow", 0.9)
            c.set(px, py, col)
            c.set(px + 1, py, col)
            c.set(px, py + 1, R("green", 0.4))
    return c


def t_sand():
    c = tile()
    noisy(c, "sand", 0.35, 0.70, 31)
    for y in range(0, H, 5):
        for x in range(W):
            if h32(x, y, 32) > 90:
                c.set(x, y + (x % 2), R("sand", 0.28))
    return c


def t_water(seed=41, shift=0):
    c = tile()
    for y in range(H):
        for x in range(W):
            v = 0.30 + 0.28 * ((x + y * 2 + shift * 3) % 9) / 9.0
            v += (h32(x, y, seed) / 255.0 - 0.5) * 0.10
            c.set(x, y, R("water", v))
    for y in range(H):
        for x in range(W):
            if h32(x, y + shift, seed + 3) > 232:
                c.set(x, y, R("water", 0.92))
    return c


def t_parking():
    c = t_asphalt(6)
    for y in range(H):
        c.set(0, y, R("white", 0.55))
        c.set(1, y, R("white", 0.42))
    for x in range(W):
        c.set(x, 15, R("white", 0.45))
    return c


def t_roof(kind):
    c = tile()
    if kind == "grey":
        noisy(c, "concrete", 0.18, 0.42, 51)
        for (px, py) in ((4, 4), (11, 11)):
            c.rect(px, py, 3, 2, R("metal", 0.55))
            c.rect(px, py + 2, 3, 1, R("metal", 0.30))
    elif kind == "red":
        for y in range(H):
            for x in range(W):
                row = y // 4
                off = (row % 2) * 2
                c.set(x, y, R("brick", 0.42 + ((x + off) % 4 == 0) * 0.12))
            c.hline(0, 15, y, R("brick", 0.24) if y % 4 == 0 else c.get(0, y))
        for y in range(0, H, 4):
            for x in range(W):
                c.set(x, y, R("brick", 0.20))
    elif kind == "teal":
        noisy(c, "teal", 0.22, 0.46, 53)
        for x in range(0, H, 5):
            for y in range(H):
                c.set(x, y, R("teal", 0.60))
    elif kind == "glass":
        for y in range(H):
            for x in range(W):
                c.set(x, y, R("glass", 0.30 + ((x // 4 + y // 4) % 2) * 0.16))
        for i in range(H):
            c.set(i, i, R("glass", 0.92))
            c.set((i + 6) % H, i, R("glass", 0.78))
        for x in range(0, H, 8):
            for y in range(H):
                c.set(x, y, R("glass", 0.18))
        for y in range(0, H, 8):
            for x in range(W):
                c.set(x, y, R("glass", 0.18))
    else:  # brown shingles
        for y in range(H):
            for x in range(W):
                c.set(x, y, R("wood", 0.30 + ((x // 2 + (y // 3) * 2) % 2) * 0.22))
        for y in range(0, H, 3):
            for x in range(W):
                c.set(x, y, R("wood", 0.18))
    return c


def t_dock():
    c = tile()
    for y in range(H):
        for x in range(W):
            c.set(x, y, R("wood", 0.40 + (y % 5) * 0.04 +
                          (h32(x, y, 61) / 255.0) * 0.10))
    for y in range(0, H, 5):
        for x in range(W):
            c.set(x, y, R("wood", 0.22))
    for x in (2, 9, 14):
        for y in range(H):
            c.set(x, y, R("wood", 0.60) if y % 5 else R("wood", 0.30))
    return c


def t_plaza():
    c = tile()
    noisy(c, "concrete", 0.34, 0.52, 71)
    for y in range(H):
        for x in range(W):
            if (x + y) % 8 == 0 or (x - y) % 8 == 0:
                c.set(x, y, R("concrete", 0.24))
    return c


def t_dirt():
    c = tile()
    noisy(c, "wood", 0.14, 0.36, 81)
    for i in range(14):
        px = h32(i, 5, 82) % W
        py = h32(i, 6, 83) % H
        c.set(px, py, R("wood", 0.62))
        c.set(px + 1, py, R("wood", 0.48))
    return c


def t_alley():
    c = t_asphalt(9)
    for y in range(H):
        for x in range(W):
            if h32(x, y, 91) > 205:
                c.set(x, y, R("asphalt", 0.14))
    for y in range(2, H, 6):
        for x in range(W):
            c.set(x, y, R("asphalt", 0.62))
    return c


BUILDERS = {
    "void": lambda: pal.Canvas(W, H, 0),
    "asphalt": t_asphalt,
    "asphalt_v": t_asphalt_v,
    "asphalt_h": t_asphalt_h,
    "crosswalk": t_crosswalk,
    "manhole": t_manhole,
    "sidewalk": t_sidewalk,
    "curb_n": lambda: curb("n"),
    "curb_s": lambda: curb("s"),
    "curb_e": lambda: curb("e"),
    "curb_w": lambda: curb("w"),
    "grass": t_grass,
    "grass2": lambda: t_grass(seed=27, flowers=True),
    "sand": t_sand,
    "water": t_water,
    "water2": lambda: t_water(seed=43, shift=1),
    "parking": t_parking,
    "roof_grey": lambda: t_roof("grey"),
    "roof_red": lambda: t_roof("red"),
    "roof_teal": lambda: t_roof("teal"),
    "roof_glass": lambda: t_roof("glass"),
    "roof_brown": lambda: t_roof("brown"),
    "dock": t_dock,
    "plaza": t_plaza,
    "dirt": t_dirt,
    "alley": t_alley,
}


def main():
    os.makedirs(GEN, exist_ok=True)
    canvases = [BUILDERS[name]() for name, _ in TILES]

    hdr = ["/* generated by tools/gentiles.py - do not edit */",
           "#ifndef GTA_TILES_H",
           "#define GTA_TILES_H",
           "",
           "#define TILE_W 16",
           "#define TILE_H 16",
           "#define NTILES %d" % len(TILES),
           "",
           "/* tile flags */",
           "#define TF_SOLID 1",
           "#define TF_ROOF  2",
           "#define TF_WATER 4",
           "",
           "enum {"]
    for i, (name, _) in enumerate(TILES):
        hdr.append("  T_%s = %d," % (name.upper(), i))
    hdr.append("};")
    hdr.append("")
    hdr.append("extern const unsigned char g_tiles[NTILES * TILE_W * TILE_H];")
    hdr.append("extern const unsigned char g_tile_flags[NTILES];")
    hdr.append("extern const unsigned char g_tile_opaque[NTILES];")
    hdr.append("")
    hdr.append("#endif")
    with open(os.path.join(GEN, "tiles.h"), "w", encoding="ascii") as f:
        f.write("\n".join(hdr) + "\n")

    blob = bytearray()
    for c in canvases:
        blob += c.px
    with open(os.path.join(GEN, "tiles.c"), "w", encoding="ascii") as f:
        f.write("/* generated by tools/gentiles.py - do not edit */\n")
        f.write("#include \"tiles.h\"\n\n")
        pal.emit_c_array(f, "g_tiles", blob, 32)
        f.write("\nconst unsigned char g_tile_flags[NTILES] = {\n  ")
        flags = []
        for _, fl in TILES:
            v = 0
            if "solid" in fl:
                v |= 1
            if "roof" in fl:
                v |= 2
            if "water" in fl:
                v |= 4
            flags.append(v)
        f.write(",".join(str(v) for v in flags))
        f.write("\n};\n")

        # A tile with no transparent pixels can be blitted with memcpy instead
        # of a per-pixel loop. That single flag is most of the render budget.
        f.write("\nconst unsigned char g_tile_opaque[NTILES] = {\n  ")
        op = []
        for c in canvases:
            op.append(1 if all(v != 0 for v in c.px) else 0)
        f.write(",".join(str(v) for v in op))
        f.write("\n};\n")
    print("wrote tiles.c/h: %d tiles, %d bytes" % (len(TILES), len(blob)))

    # preview sheet, useful when a tile looks like a bruised waffle
    sh = pal.sheet(canvases, scale=4, cols=9)
    if sh:
        sh.save(os.path.join(ROOT, "build", "preview_tiles.png"))


if __name__ == "__main__":
    main()

"""Generate the README banner and the repository icon.

Same rules as the rest of the project: no hand-drawn binary assets. The banner
is composed from the game's own palette and the game's own CP866 font, at
320x140 logical pixels, then upscaled with nearest-neighbour so the pixels stay
pixels.

  docs/banner.png   960x420  README hero
  docs/icon.png     256x256  square badge (repo avatar)
  docs/icon.ico     16/32/48/64/128/256 for Windows shortcuts
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pal  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")
DOCS = os.path.join(ROOT, "docs")

P = pal.PAL
RGB = pal.rgb24()

BANNER_W, BANNER_H = 320, 212
ICON_W, ICON_H = 64, 64
SCALE = 3
ICON_SCALE = 4


# --------------------------------------------------------------------- font ---
def load_font():
    """The 256x16 glyph blob. Prefer the generated file; fall back to Unifont."""
    path = os.path.join(GEN, "font.c")
    table = bytearray(256 * 16)
    if os.path.exists(path):
        src = open(path, encoding="ascii").read()
        vals = []
        for line in src.splitlines():
            if "/*" in line and "0x" in line:
                body = line.split("/*")[0]
                for tok in body.split(","):
                    tok = tok.strip()
                    if tok.startswith("0x"):
                        vals.append(int(tok, 16))
        for i, v in enumerate(vals[:256 * 16]):
            table[i] = v
        return table
    # no generated font yet: read Unifont directly
    import gzip
    import vendor
    uf = vendor.unifont()
    if not uf:
        raise SystemExit("no font: run tools/genfont.py first")
    glyphs = {}
    with gzip.open(uf, "rt", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            cp, data = line.split(":")
            glyphs[int(cp, 16)] = data
    for code in range(32, 256):
        try:
            ch = bytes([code]).decode("cp866")
        except UnicodeDecodeError:
            continue
        data = glyphs.get(ord(ch))
        if not data:
            continue
        rows = [int(data[i:i + 2], 16) for i in range(0, len(data), 2)]
        if len(rows) == 32:
            rows = [rows[i * 2 + 1] for i in range(16)]
        if len(rows) == 16:
            table[code * 16:code * 16 + 16] = bytes(rows)
    return table


FONT = load_font()


def glyph_bits(ch):
    """8x16 boolean grid for one CP866 byte."""
    base = (ord(ch) & 0xFF) * 16
    grid = [[False] * 8 for _ in range(16)]
    for y in range(16):
        bits = FONT[base + y]
        for x in range(8):
            grid[y][x] = bool(bits & (0x80 >> x))
    return grid


def dilate(grid):
    """1px outline mask around a glyph grid."""
    out = [[False] * 10 for _ in range(18)]
    for y in range(16):
        for x in range(8):
            if not grid[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    out[y + 1 + dy][x + 1 + dx] = True
    return out


def draw_text(cv, x, y, s, scale, ramp, outline=None, shadow=None,
              shadow_off=1):
    """Draw a string with an optional outline and drop shadow."""
    cx = x
    for ch in s:
        g = glyph_bits(ch)
        if shadow is not None:
            ox = cx + shadow_off * scale
            oy = y + shadow_off * scale
            for gy in range(16):
                for gx in range(8):
                    if not g[gy][gx]:
                        continue
                    for dy in range(scale):
                        for dx in range(scale):
                            cv.set(ox + gx * scale + dx, oy + gy * scale + dy,
                                   shadow)
        if outline is not None:
            d = dilate(g)
            for gy in range(18):
                for gx in range(10):
                    if not d[gy][gx]:
                        continue
                    px = cx - scale + gx * scale
                    py = y - scale + gy * scale
                    for dy in range(scale):
                        for dx in range(scale):
                            cv.set(px + dx, py + dy, outline)
        base, n = ramp
        for gy in range(16):
            # vertical gradient inside one ramp, never across ramps
            step = (n - 1) - (gy * n) // 16
            col = base + max(0, min(n - 1, step))
            for gx in range(8):
                if not g[gy][gx]:
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        cv.set(cx + gx * scale + dx, y + gy * scale + dy, col)
        cx += 8 * scale
    return cx


def text_width(s, scale):
    return len(s) * 8 * scale


# ------------------------------------------------------------------- pieces ---
def sky(cv, w, h):
    """Sunset, banded by hand so it reads as 1994 and not as a Photoshop
    gradient with too many colours."""
    bands = [
        (0.00, 0.20, "void", 2), (0.20, 0.34, "purple", 3),
        (0.34, 0.46, "purple", 6), (0.46, 0.56, "pink", 3),
        (0.56, 0.66, "pink", 6), (0.66, 0.76, "orange", 3),
        (0.76, 0.84, "orange", 6), (0.84, 1.00, "yellow", 4),
    ]
    for t0, t1, ramp, step in bands:
        y0 = int(t0 * h)
        y1 = int(t1 * h)
        base, n = P[ramp + "0"], pal.RAMPS[ramp][1]
        col = base + min(n - 1, step)
        for y in range(y0, y1):
            cv.hline(0, w - 1, y, col)


def sun(cv, cx, cy, r):
    """A retro sun: disc with horizontal cuts taken out of the bottom half."""
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 > r * r:
                continue
            d = y - cy
            if d > 0:
                # stripes get thicker towards the bottom
                period = 3 + d // 3
                if d % period == 0:
                    continue
            t = (y - (cy - r)) / (2.0 * r)
            step = 3 + int(t * 4)
            col = P["yellow0"] + min(pal.RAMPS["yellow"][1] - 1, step)
            if t > 0.55:
                col = P["orange0"] + min(pal.RAMPS["orange"][1] - 1, 5)
            cv.set(x, y, col)


def skyline(cv, w, base_y, rng_seed=7):
    """Deterministic silhouette with lit windows. Deterministic because a
    banner that changes every build is a banner nobody can diff."""
    n = rng_seed
    x = -4
    while x < w:
        n = (n * 1103515245 + 12345) & 0x7FFFFFFF
        bw = 8 + (n >> 8) % 14
        n = (n * 1103515245 + 12345) & 0x7FFFFFFF
        bh = 6 + (n >> 8) % 26
        top = base_y - bh
        for y in range(top, base_y):
            cv.hline(x, x + bw - 1, y, P["void0"] if y < top + 2 else P["void1"])
        # windows
        for wy in range(top + 3, base_y - 2, 4):
            for wx in range(x + 2, x + bw - 1, 3):
                n = (n * 1103515245 + 12345) & 0x7FFFFFFF
                if (n >> 16) % 100 < 26:
                    cv.set(wx, wy, P["yellow4"])
        x += bw + 2


def water(cv, w, y0, y1):
    for y in range(y0, y1):
        t = (y - y0) / max(1.0, (y1 - y0))
        step = 1 + int(t * 5)
        col = P["water0"] + min(pal.RAMPS["water"][1] - 1, step)
        cv.hline(0, w - 1, y, col)
    # highlights
    n = 11
    for i in range(26):
        n = (n * 1103515245 + 12345) & 0x7FFFFFFF
        hy = y0 + 2 + (n >> 8) % max(1, (y1 - y0 - 3))
        n = (n * 1103515245 + 12345) & 0x7FFFFFFF
        hx = (n >> 8) % w
        n = (n * 1103515245 + 12345) & 0x7FFFFFFF
        hw = 2 + (n >> 8) % 7
        cv.hline(hx, min(w - 1, hx + hw), hy, P["water6"])


def palm(cv, x, y, h=22, flip=False, col=None):
    """Silhouette palm: a curved trunk and drooping fronds.

    Fronds are quadratic arcs, not straight lines: a straight line reads as a
    rake, and a rake is the other project.

    `col` defaults to pure black, which is a mistake over a black skyline --
    the tree disappears. Callers that place a palm against buildings should
    pass a dark green instead.
    """
    col = P["void0"] if col is None else col
    sgn = 1 if flip else -1
    trunk = []
    for i in range(h):
        t = i / float(h)
        xx = x + int(t * t * 5) * sgn
        trunk.append((xx, y - i))
        cv.set(xx, y - i, col)
        if i < h - 2:
            cv.set(xx + 1, y - i, col)

    top_x, top_y = trunk[-1]
    # (fx, fy, length) -- up, up-out, out, down-out
    fronds = [(-1, -2, 10), (1, -2, 10), (-2, -1, 11), (2, -1, 11),
              (-2, 1, 9), (2, 1, 9), (0, -3, 7)]
    for fx, fy, ln in fronds:
        for t in range(1, ln + 1):
            px = top_x + (fx * t * sgn if fx else 0)
            py = top_y + fy * t // 2 + (t * t) // 8
            # a frond is a 2px-thick arc; 1px reads as a dashed line
            cv.set(px, py, col)
            cv.set(px, py + 1, col)
            if t < ln - 3:
                cv.set(px + sgn, py, col)
    for dy in range(2):
        for dx in range(2):
            cv.set(top_x + dx, top_y + dy, col)


def beach(cv, w, y0, y1):
    for y in range(y0, y1):
        t = (y - y0) / max(1.0, (y1 - y0))
        step = 2 + int(t * 4)
        col = P["sand0"] + min(pal.RAMPS["sand"][1] - 1, step)
        cv.hline(0, w - 1, y, col)


# ------------------------------------------------------------------- banner ---
def make_banner():
    cv = pal.Canvas(BANNER_W, BANNER_H, P["void0"])

    # Zones, top to bottom. Every element gets its own band so nothing has to
    # be composited over anything: the last attempt put a text plate over the
    # skyline and produced a row of floating building tops.
    SKY_Y = 100
    WATER_Y = 114
    SAND_Y = 124
    BAND_Y = 136          # "DOS CITY" plate
    BAND_H = 32
    STRIP_Y = BAND_Y + BAND_H

    sky(cv, BANNER_W, SKY_Y)
    sun(cv, 266, 54, 18)
    skyline(cv, BANNER_W, SKY_Y, 7)
    water(cv, BANNER_W, SKY_Y, WATER_Y)
    beach(cv, BANNER_W, WATER_Y, SAND_Y)

    # palms in the foreground corners, standing on the sand. Dark green, not
    # black: a black palm against a black skyline is an invisible palm.
    palm(cv, 18, SAND_Y - 1, 30, flip=False, col=P["darkgreen3"])
    palm(cv, 302, SAND_Y - 1, 26, flip=True, col=P["darkgreen3"])

    # logo: gold gradient, hard outline, offset shadow
    title = "GTA VI"
    tw = text_width(title, 4)
    draw_text(cv, (BANNER_W - tw) // 2, 0, title, 4,
              (P["gold0"], pal.RAMPS["gold"][1]),
              outline=P["void0"], shadow=P["void0"], shadow_off=1)

    # subtitle plate: its own band, with a lit top edge
    for y in range(BAND_Y, STRIP_Y):
        cv.hline(0, BANNER_W - 1, y, P["void0"])
    cv.hline(0, BANNER_W - 1, BAND_Y, P["purple3"])
    cv.hline(0, BANNER_W - 1, BAND_Y + 1, P["purple1"])
    cv.hline(0, BANNER_W - 1, STRIP_Y - 1, P["void2"])
    sub = "DOS CITY"
    sw = text_width(sub, 2)
    draw_text(cv, (BANNER_W - sw) // 2, BAND_Y + 6, sub, 2,
              (P["neoncyan0"], pal.RAMPS["neoncyan"][1]), outline=P["void0"])

    # bottom strip: the honest specs
    tag = "VICE SHORES, LEONIDA. 1994."
    draw_text(cv, (BANNER_W - text_width(tag, 1)) // 2, STRIP_Y + 6, tag, 1,
              (P["concrete5"], pal.RAMPS["concrete"][1]))
    specs = "320x200  256 COLORS  DJGPP  PC SPEAKER"
    draw_text(cv, (BANNER_W - text_width(specs, 1)) // 2, STRIP_Y + 24, specs,
              1, (P["concrete3"], pal.RAMPS["concrete"][1]))

    return cv


# --------------------------------------------------------------------- icon ---
def make_icon():
    cv = pal.Canvas(ICON_W, ICON_H, 0)

    # rounded-square badge
    r = 10
    inside = [[False] * ICON_W for _ in range(ICON_H)]
    for y in range(ICON_H):
        for x in range(ICON_W):
            dx = min(x, ICON_W - 1 - x)
            dy = min(y, ICON_H - 1 - y)
            if dx >= r or dy >= r:
                inside[y][x] = True
            elif (r - dx) ** 2 + (r - dy) ** 2 <= r * r + r:
                inside[y][x] = True

    # sunset inside the badge
    bands = [(0.00, 0.24, "purple", 2), (0.24, 0.38, "purple", 6),
             (0.38, 0.50, "pink", 4), (0.50, 0.62, "orange", 4),
             (0.62, 0.74, "orange", 7), (0.74, 0.84, "yellow", 4)]
    for t0, t1, ramp, step in bands:
        base, n = P[ramp + "0"], pal.RAMPS[ramp][1]
        col = base + min(n - 1, step)
        for y in range(int(t0 * ICON_H), int(t1 * ICON_H)):
            for x in range(ICON_W):
                if inside[y][x]:
                    cv.set(x, y, col)

    # sun: disc with the classic horizontal cuts, sitting on the horizon.
    # Kept below the monogram so the two do not fight for the same pixels.
    for y in range(38, 54):
        for x in range(ICON_W):
            if not inside[y][x]:
                continue
            if (x - 32) ** 2 + (y - 50) ** 2 <= 100:
                d = y - 50
                if d > 0 and d % (2 + d // 5) == 0:
                    continue
                cv.set(x, y, P["yellow6"] if y < 48 else P["orange6"])

    # sea, then the badge floor
    for y in range(52, 60):
        for x in range(ICON_W):
            if inside[y][x]:
                cv.set(x, y, P["water3"])
    for y in range(60, ICON_H):
        for x in range(ICON_W):
            if inside[y][x]:
                cv.set(x, y, P["void1"])

    # monogram: a big gold "6" (GTA VI = GTA 6) with "DOS" under it.
    # "VI" was the first attempt and it read as "VJ": Unifont's capital I has
    # serifs, and at 64x64 a serif is the whole letter.
    draw_text(cv, (ICON_W - text_width("6", 3)) // 2, 2, "6", 3,
              (P["gold0"], pal.RAMPS["gold"][1]), outline=P["void0"],
              shadow=P["void0"], shadow_off=1)
    draw_text(cv, (ICON_W - text_width("DOS", 1)) // 2, 31, "DOS", 1,
              (P["neoncyan0"], pal.RAMPS["neoncyan"][1]),
              outline=P["void0"])

    # bezel: light top-left, dark bottom-right
    for y in range(ICON_H):
        for x in range(ICON_W):
            if not inside[y][x]:
                continue
            edge = (not (x + 1 < ICON_W and inside[y][x + 1]) or
                    not (y + 1 < ICON_H and inside[y + 1][x]) or
                    not (x - 1 >= 0 and inside[y][x - 1]) or
                    not (y - 1 >= 0 and inside[y - 1][x]))
            if edge:
                light = (y < ICON_H // 2) or (x < ICON_W // 2)
                cv.set(x, y, P["concrete5"] if light else P["concrete2"])
    return cv, inside


def to_rgba(cv, mask=None):
    im = Image.new("RGBA", (cv.w, cv.h), (0, 0, 0, 0))
    px = im.load()
    for y in range(cv.h):
        for x in range(cv.w):
            if mask is not None and not mask[y][x]:
                continue
            c = cv.px[y * cv.w + x]
            if c == 0:
                continue
            px[x, y] = (RGB[c * 3], RGB[c * 3 + 1], RGB[c * 3 + 2], 255)
    return im


def main():
    os.makedirs(DOCS, exist_ok=True)

    banner = make_banner()
    big = to_rgba(banner).resize(
        (BANNER_W * SCALE, BANNER_H * SCALE), Image.NEAREST)
    big.convert("RGB").save(os.path.join(DOCS, "banner.png"))
    print("wrote docs/banner.png %dx%d" % big.size)

    icon_cv, mask = make_icon()
    icon = to_rgba(icon_cv, mask)
    icon_big = icon.resize((ICON_W * ICON_SCALE, ICON_H * ICON_SCALE),
                           Image.NEAREST)
    icon_big.save(os.path.join(DOCS, "icon.png"))
    print("wrote docs/icon.png %dx%d" % icon_big.size)

    icon_big.save(os.path.join(DOCS, "icon.ico"),
                  sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128),
                         (256, 256)])
    print("wrote docs/icon.ico")


if __name__ == "__main__":
    main()

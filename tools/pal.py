"""Palette + a tiny indexed-art toolkit for GTA VI: DOS CITY.

Same idea as DOSYA: the palette is built from named ramps, so sprite code says
"the 4th asphalt" instead of hunting for a magic index.  Everything is emitted
as 6-bit VGA DAC values, and index 0 is always transparent.

The ramps are tuned for a Leonida dusk: hot asphalt, teal glass, neon pink.
"""
import math

try:
    from PIL import Image
except ImportError:          # the C build still works, previews do not
    Image = None

# --- ramps: (stops, count). stops are RGB 0..255, sampled to `count` entries ---
RAMPS = {
    "void":      ([(6, 4, 12), (22, 16, 38), (44, 32, 66)], 8),
    "asphalt":   ([(26, 24, 30), (58, 56, 66), (96, 94, 108)], 8),
    "concrete":  ([(52, 50, 56), (112, 110, 118), (176, 174, 182)], 10),
    "white":     ([(150, 148, 156), (250, 246, 236)], 8),
    "grey":      ([(16, 16, 22), (86, 86, 96), (168, 168, 178)], 10),
    "skin":      ([(104, 58, 44), (206, 142, 104), (255, 208, 170)], 8),
    "red":       ([(64, 8, 14), (176, 28, 34), (250, 96, 78)], 10),
    "blood":     ([(30, 2, 6), (92, 10, 14), (146, 26, 24)], 6),
    "orange":    ([(88, 38, 6), (222, 118, 22), (255, 196, 110)], 8),
    "yellow":    ([(96, 72, 6), (232, 190, 28), (255, 246, 156)], 8),
    "gold":      ([(84, 52, 4), (206, 150, 22), (255, 232, 130)], 8),
    "green":     ([(14, 44, 16), (46, 112, 40), (132, 194, 74)], 10),
    "darkgreen": ([(4, 20, 10), (20, 56, 28), (44, 94, 48)], 8),
    "palm":      ([(16, 48, 22), (40, 104, 44), (86, 156, 62)], 8),
    "sand":      ([(96, 76, 46), (186, 158, 108), (240, 220, 170)], 8),
    "water":     ([(6, 26, 52), (18, 74, 118), (54, 140, 180)], 10),
    "teal":      ([(4, 40, 44), (22, 108, 110), (78, 186, 176)], 8),
    "sky":       ([(26, 30, 84), (86, 118, 200), (176, 210, 250)], 8),
    "blue":      ([(8, 14, 58), (40, 74, 168), (108, 156, 240)], 8),
    "navy":      ([(4, 6, 24), (18, 26, 60), (38, 52, 104)], 8),
    "purple":    ([(26, 10, 40), (80, 34, 112), (146, 84, 186)], 10),
    "pink":      ([(104, 30, 62), (212, 96, 132), (255, 178, 200)], 8),
    "neonpink":  ([(92, 6, 44), (226, 30, 118), (255, 132, 196)], 8),
    "neoncyan":  ([(4, 56, 68), (26, 168, 190), (150, 250, 255)], 8),
    "metal":     ([(26, 28, 34), (98, 104, 118), (196, 204, 216)], 8),
    "glass":     ([(10, 22, 40), (32, 74, 108), (86, 150, 178)], 8),
    "brick":     ([(44, 20, 16), (118, 54, 38), (176, 96, 66)], 8),
    "wood":      ([(38, 22, 14), (104, 64, 34), (166, 116, 70)], 8),
    "fire":      ([(104, 12, 6), (238, 112, 18), (255, 238, 156)], 8),
}

PAL = {}
ORDER = []


def _build():
    idx = 1                       # 0 stays transparent / black
    for name, (stops, n) in RAMPS.items():
        for i in range(n):
            t = i / (n - 1) if n > 1 else 0.0
            seg = t * (len(stops) - 1)
            k = min(int(seg), len(stops) - 2)
            f = seg - k
            a, b = stops[k], stops[k + 1]
            rgb = tuple(int(round(a[c] + (b[c] - a[c]) * f)) for c in range(3))
            PAL["%s%d" % (name, i)] = idx
            ORDER.append((idx, rgb))
            idx += 1
    return idx


_used = _build()
BLACK = 0
TRANSPARENT = 0


def pal_rgb():
    """256*3 bytes of 6-bit DAC values, ready for the VGA."""
    out = bytearray(768)
    for i, (r, g, b) in ORDER:
        out[i * 3 + 0] = min(63, r >> 2)
        out[i * 3 + 1] = min(63, g >> 2)
        out[i * 3 + 2] = min(63, b >> 2)
    return out


def rgb24():
    out = bytearray(768)
    for i, (r, g, b) in ORDER:
        out[i * 3 + 0] = r
        out[i * 3 + 1] = g
        out[i * 3 + 2] = b
    return out


def C(name):
    return PAL[name]


# --- ramp introspection, used to build darker copies and shadows -------------
RAMP_INFO = {}
for _name, (_stops, _n) in RAMPS.items():
    for _i in range(_n):
        RAMP_INFO[PAL["%s%d" % (_name, _i)]] = (_name, _i, _n)


def dim_index(idx, steps=3):
    """Step down a ramp (never below 0). Index 0 stays transparent."""
    if idx == 0:
        return 0
    info = RAMP_INFO.get(idx)
    if not info:
        return idx
    name, i, n = info
    return PAL["%s%d" % (name, max(0, i - steps))]


def ramp(name, n, t):
    """Sample ramp entry by t in 0..1."""
    i = max(0, min(n - 1, int(t * (n - 1) + 0.5)))
    return C("%s%d" % (name, i))


class Canvas:
    """Indexed 8-bit canvas. 0 is transparent."""

    def __init__(self, w, h, fill=0):
        self.w, self.h = w, h
        self.px = bytearray([fill]) * (w * h)

    def clone(self):
        c = Canvas(self.w, self.h)
        c.px = bytearray(self.px)
        return c

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = c

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y * self.w + x]
        return 0

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy, c)

    def hline(self, x0, x1, y, c):
        for x in range(x0, x1 + 1):
            self.set(x, y, c)

    def vline(self, x, y0, y1, c):
        for y in range(y0, y1 + 1):
            self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def disc(self, cx, cy, r, c):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    self.set(x, y, c)

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(cy - ry, cy + ry + 1):
            for x in range(cx - rx, cx + rx + 1):
                if rx and ry and ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.set(x, y, c)

    def blit(self, other, dx, dy, skip_zero=True):
        for y in range(other.h):
            for x in range(other.w):
                c = other.px[y * other.w + x]
                if skip_zero and c == 0:
                    continue
                self.set(dx + x, dy + y, c)

    def flip_h(self):
        out = Canvas(self.w, self.h)
        for y in range(self.h):
            for x in range(self.w):
                out.px[y * self.w + (self.w - 1 - x)] = self.px[y * self.w + x]
        return out

    def flip_v(self):
        out = Canvas(self.w, self.h)
        for y in range(self.h):
            out.px[(self.h - 1 - y) * self.w:(self.h - y) * self.w] = \
                self.px[y * self.w:(y + 1) * self.w]
        return out

    def rotate(self, deg):
        """Nearest-neighbour rotation of the indices. Small sprites only."""
        a = math.radians(deg)
        ca, sa = math.cos(a), math.sin(a)
        cx, cy = self.w / 2.0, self.h / 2.0
        # new bounding box
        corners = [(-cx, -cy), (cx, -cy), (-cx, cy), (cx, cy)]
        nx = [x * ca - y * sa for x, y in corners]
        ny = [x * sa + y * ca for x, y in corners]
        nw = int(math.ceil(max(nx) - min(nx))) + 1
        nh = int(math.ceil(max(ny) - min(ny))) + 1
        out = Canvas(nw, nh)
        ocx, ocy = nw / 2.0, nh / 2.0
        for y in range(nh):
            for x in range(nw):
                sx = (x - ocx) * ca + (y - ocy) * sa + cx
                sy = -(x - ocx) * sa + (y - ocy) * ca + cy
                ix, iy = int(math.floor(sx + 0.5)), int(math.floor(sy + 0.5))
                if 0 <= ix < self.w and 0 <= iy < self.h:
                    out.px[y * nw + x] = self.px[iy * self.w + ix]
        return out

    def outline(self, color, diagonal=False):
        """1px outline around every non-transparent pixel."""
        src = bytearray(self.px)
        for y in range(self.h):
            for x in range(self.w):
                if src[y * self.w + x]:
                    continue
                nb = [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)]
                if diagonal:
                    nb += [(x - 1, y - 1), (x + 1, y - 1),
                           (x - 1, y + 1), (x + 1, y + 1)]
                for nx, ny in nb:
                    if 0 <= nx < self.w and 0 <= ny < self.h and \
                            src[ny * self.w + nx]:
                        self.px[y * self.w + x] = color
                        break
        return self

    def from_ascii(self, rows, legend):
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch == "." or ch == " ":
                    continue
                v = legend[ch]
                self.set(x, y, PAL[v] if isinstance(v, str) else v)
        return self

    def to_image(self):
        if Image is None:
            return None
        rgb = rgb24()
        im = Image.new("RGB", (self.w, self.h))
        px = im.load()
        for y in range(self.h):
            for x in range(self.w):
                c = self.px[y * self.w + x]
                px[x, y] = (rgb[c * 3], rgb[c * 3 + 1], rgb[c * 3 + 2])
        return im


def sheet(canvases, scale=4, cols=8, bg=(26, 20, 34)):
    """Contact sheet of canvases, scaled up for eyeballing."""
    if Image is None:
        return None
    if not canvases:
        return Image.new("RGB", (10, 10))
    cw = max(c.w for c in canvases) + 2
    ch = max(c.h for c in canvases) + 2
    cols = max(1, min(cols, len(canvases)))
    rows = (len(canvases) + cols - 1) // cols
    out = Image.new("RGB", (cols * cw, rows * ch), bg)
    for i, c in enumerate(canvases):
        ox = (i % cols) * cw + 1
        oy = (i // cols) * ch + 1
        out.paste(c.to_image(), (ox, oy))
    return out.resize((out.width * scale, out.height * scale), Image.NEAREST)


def emit_c_array(f, name, data, per_line=24):
    f.write("const unsigned char %s[%d] = {\n" % (name, len(data)))
    for i in range(0, len(data), per_line):
        f.write("  " + ",".join("%d" % b for b in data[i:i + per_line]) + ",\n")
    f.write("};\n")

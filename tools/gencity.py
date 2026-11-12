"""City generator -> build/gen/city.c + city.h.

Vice Shores, Leonida. 96x96 tiles of 16px = a 1536x1536 pixel city, which is
2.3 MB of screen if you unrolled it and about 9 KB in the ROM, which is why
1994 was a good year for game design.

Layout: a 6x6 grid of blocks separated by 4-tile roads. Districts are picked
from a table, not from a noise function, because a city should look *designed*
even when it is generated. The east column is the beach, the south row is the
docks, the middle is downtown.

The generator refuses to emit a city where a mission marker is walled in:
every point of interest must be reachable on foot from the spawn, and that is
checked with a flood fill, not with hope.
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gentiles  # noqa: E402
import genart    # noqa: E402
import pal       # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")

W = H = 96
CELL = 16
ROAD = 4
BLOCK = 12
NC = 6

T = {name: i for i, (name, _) in enumerate(gentiles.TILES)}
SOLID = {name for name, fl in gentiles.TILES if "solid" in fl}
WATER = {name for name, fl in gentiles.TILES if "water" in fl}

# sprite bases, mirroring the order genart.py emits
SPR_HUMAN = 0
SPR_CAR = len(genart.HUMANS) * 4
SPR_PROP = SPR_CAR + len(genart.CARS) * genart.CAR_DIRS
PROP = {"palm1": 0, "palm2": 1, "bush": 2, "bush2": 3, "barrel": 4,
        "dumpster": 5, "lamp": 6, "billboard": 7}

ZONE_DOWNTOWN = 0
ZONE_SUBURB = 1
ZONE_BEACH = 2
ZONE_DOCKS = 3
ZONE_PARK = 4
ZONE_IND = 5

LAYOUT = [
    # row 0
    ["tower", "tower", "plaza", "parking", "beach", "beach"],
    # row 1
    ["tower", "park", "houses", "houses", "beach", "beach"],
    # row 2
    ["parking", "houses", "tower", "mall", "beach", "beach"],
    # row 3
    ["houses", "park", "tower", "parking", "beach", "beach"],
    # row 4
    ["houses", "mall", "park", "houses", "beach", "beach"],
    # row 5
    ["airport", "dock", "dock", "dock", "dock", "water"],
]

ZONE_OF = {
    "tower": ZONE_DOWNTOWN, "mall": ZONE_DOWNTOWN, "plaza": ZONE_DOWNTOWN,
    "houses": ZONE_SUBURB, "park": ZONE_PARK, "parking": ZONE_IND,
    "beach": ZONE_BEACH, "dock": ZONE_DOCKS, "airport": ZONE_IND,
    "water": ZONE_DOCKS,
}


class City:
    def __init__(self):
        self.t = bytearray([T["asphalt"]] * (W * H))
        self.zone = bytearray(W * H)
        self.props = []          # (x_px, y_px, sprite)
        self.car_spawns = []     # (x_px, y_px, angle8, model)
        self.ped_spawns = []     # (x_px, y_px)
        self.pickups = []        # (x_px, y_px, kind)
        self.rng = random.Random(19941)

    # ---- primitives
    def set(self, x, y, name):
        if 0 <= x < W and 0 <= y < H:
            self.t[y * W + x] = T[name]

    def get(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            return self.t[y * W + x]
        return T["void"]

    def fill(self, x0, y0, x1, y1, name):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, name)

    def ring(self, x0, y0, x1, y1, name):
        for x in range(x0, x1 + 1):
            self.set(x, y0, name)
            self.set(x, y1, name)
        for y in range(y0, y1 + 1):
            self.set(x0, y, name)
            self.set(x1, y, name)

    def zone_fill(self, x0, y0, x1, y1, z):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if 0 <= x < W and 0 <= y < H:
                    self.zone[y * W + x] = z

    def px(self, tx, ty):
        return tx * 16 + 8, ty * 16 + 8

    # ---- roads ---------------------------------------------------------
    def roads(self):
        for y in range(H):
            for x in range(W):
                if (x % CELL) < ROAD or (y % CELL) < ROAD:
                    self.set(x, y, "asphalt")
        # lane paint: dashes along the middle of each road
        for cy in range(NC):
            for cx in range(NC):
                x0, y0 = cx * CELL, cy * CELL
                for y in range(y0 + ROAD, y0 + CELL):
                    for x in (x0 + 1, x0 + 2):
                        self.set(x, y, "asphalt_v")
                for x in range(x0 + ROAD, x0 + CELL):
                    for y in (y0 + 1, y0 + 2):
                        self.set(x, y, "asphalt_h")
        # zebra crossings at the intersection corners, where a real city puts
        # them. Painting them along every block edge instead makes the map
        # look like a barcode, which is not the vibe.
        for cy in range(NC + 1):
            for cx in range(NC + 1):
                ix, iy = cx * CELL, cy * CELL
                if ix + ROAD >= W or iy + ROAD >= H:
                    continue
                for x in range(ix, ix + ROAD):
                    for y in (iy + ROAD, iy + ROAD + 1):
                        if (x - ix) % 2 == 0:
                            self.set(x, y, "crosswalk")
                    for y in (iy - 2, iy - 1):
                        if (x - ix) % 2 == 0:
                            self.set(x, y, "crosswalk")
                for y in range(iy, iy + ROAD):
                    for x in (ix + ROAD, ix + ROAD + 1):
                        if (y - iy) % 2 == 0:
                            self.set(x, y, "crosswalk")
                    for x in (ix - 2, ix - 1):
                        if (y - iy) % 2 == 0:
                            self.set(x, y, "crosswalk")
        # a few manholes, for the puddles of character
        for i in range(90):
            x = self.rng.randrange(W)
            y = self.rng.randrange(H)
            if self.get(x, y) == T["asphalt"]:
                self.set(x, y, "manhole")

    # ---- block painters ------------------------------------------------
    def sidewalk(self, bx, by):
        self.fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, "sidewalk")
        for x in range(bx, bx + BLOCK):
            self.set(x, by, "curb_n")
            self.set(x, by + BLOCK - 1, "curb_s")
        for y in range(by, by + BLOCK):
            self.set(bx, y, "curb_w")
            self.set(bx + BLOCK - 1, y, "curb_e")
        self.set(bx, by, "curb_n")
        self.set(bx + BLOCK - 1, by, "curb_n")
        self.set(bx, by + BLOCK - 1, "curb_s")
        self.set(bx + BLOCK - 1, by + BLOCK - 1, "curb_s")
        # streetlights on two corners, like every block in every city
        for (tx, ty) in ((bx + 1, by + 1), (bx + BLOCK - 2, by + BLOCK - 2)):
            px, py = self.px(tx, ty)
            self.props.append((px, py, SPR_PROP + PROP["lamp"]))

    def p_tower(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "alley")
        kind = self.rng.choice(["roof_glass", "roof_grey", "roof_teal"])
        if self.rng.random() < 0.5:
            # one fat tower with a lobby strip
            self.fill(ix0 + 1, iy0 + 1, ix1 - 1, iy1 - 1, kind)
            self.fill(ix0 + 1, iy1 - 1, ix1 - 1, iy1 - 1, "roof_grey")
            px, py = self.px(ix0 + 2, iy1 - 2)
            self.props.append((px, py, SPR_PROP + PROP["dumpster"]))
        else:
            # two skinny towers and an alley between them
            mid = (iy0 + iy1) // 2
            self.fill(ix0 + 1, iy0 + 1, ix1 - 1, mid - 1, kind)
            self.fill(ix0 + 1, mid + 1, ix1 - 1, iy1 - 1,
                      "roof_grey" if kind != "roof_grey" else "roof_teal")
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_DOWNTOWN)

    def p_houses(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "grass")
        roofs = ["roof_brown", "roof_red", "roof_teal"]
        for gy in range(2):
            for gx in range(2):
                ox = ix0 + gx * 5
                oy = iy0 + gy * 5
                if ox + 3 > ix1 or oy + 3 > iy1:
                    continue
                if self.rng.random() < 0.18:
                    continue
                roof = self.rng.choice(roofs)
                self.fill(ox, oy, ox + 3, oy + 2, roof)
                self.fill(ox, oy + 3, ox + 3, oy + 3, "dirt")
                # a palm or a bush in the yard
                if self.rng.random() < 0.55:
                    px, py = self.px(ox + 4, oy + 1)
                    self.props.append((px, py, SPR_PROP +
                                       PROP[self.rng.choice(["palm1", "bush", "bush2"])]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_SUBURB)

    def p_park(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "grass")
        for y in range(iy0, iy1 + 1):
            for x in range(ix0, ix1 + 1):
                if self.rng.random() < 0.10:
                    self.set(x, y, "grass2")
        # a cross path, because parks need a reason to be crossed
        mid = (ix0 + ix1) // 2
        for x in range(ix0, ix1 + 1):
            self.set(x, mid, "plaza")
        for y in range(iy0, iy1 + 1):
            self.set(mid, y, "plaza")
        for i in range(7):
            px, py = self.px(self.rng.randrange(ix0, ix1 + 1),
                             self.rng.randrange(iy0, iy1 + 1))
            self.props.append((px, py, SPR_PROP +
                               PROP[self.rng.choice(["palm1", "palm2", "bush"])]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_PARK)

    def p_parking(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "parking")
        for y in range(iy0, iy1 + 1):
            if (y - iy0) % 3 == 0:
                for x in range(ix0, ix1 + 1):
                    self.set(x, y, "asphalt")
        # parked cars nose-in along the north edge
        for i in range(4):
            tx = ix0 + 1 + i * 2
            px, py = self.px(tx, iy0 + 1)
            self.car_spawns.append((px, py, 192, self.rng.randrange(3)))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_IND)

    def p_plaza(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "plaza")
        # fountain-ish thing made of water tiles, because 1994
        cx, cy = (ix0 + ix1) // 2, (iy0 + iy1) // 2
        self.fill(cx - 2, cy - 2, cx + 2, cy + 2, "water")
        self.ring(cx - 3, cy - 3, cx + 3, cy + 3, "roof_grey")
        for (tx, ty) in ((ix0 + 1, iy0 + 1), (ix1 - 1, iy1 - 1)):
            px, py = self.px(tx, ty)
            self.props.append((px, py, SPR_PROP + PROP["billboard"]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_DOWNTOWN)

    def p_mall(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "roof_grey")
        self.fill(ix0 + 1, iy0 + 1, ix1 - 1, iy1 - 1, "roof_glass")
        # a loading bay at the south end, cut through to the sidewalk so it is
        # not a sealed pocket that the reachability check will trip over
        self.fill(ix0 + 2, iy1 - 1, ix1 - 2, iy1 + 1, "alley")
        px, py = self.px(ix0 + 1, iy1 - 1)
        self.props.append((px, py, SPR_PROP + PROP["dumpster"]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_DOWNTOWN)

    def p_beach(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        split = ix0 + 4
        self.fill(ix0, iy0, split, iy1, "sand")
        self.fill(split + 1, iy0, ix1, iy1, "water")
        for y in range(iy0, iy1 + 1):
            if self.rng.random() < 0.35:
                self.set(split + 1, y, "water2")
        for i in range(5):
            px, py = self.px(self.rng.randrange(ix0, split + 1),
                             self.rng.randrange(iy0, iy1 + 1))
            self.props.append((px, py, SPR_PROP +
                               PROP[self.rng.choice(["palm1", "palm2"])]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_BEACH)

    def p_dock(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        split = iy0 + 5
        self.fill(ix0, iy0, ix1, split, "dock")
        self.fill(ix0, split + 1, ix1, iy1, "water")
        # a pier sticking into the water
        pier = (ix0 + ix1) // 2
        self.fill(pier, split + 1, pier + 1, iy1, "dock")
        for i in range(4):
            px, py = self.px(self.rng.randrange(ix0, ix1 + 1),
                             self.rng.randrange(iy0, split + 1))
            self.props.append((px, py, SPR_PROP +
                               PROP[self.rng.choice(["barrel", "dumpster"])]))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_DOCKS)

    def p_airport(self, bx, by):
        self.sidewalk(bx, by)
        ix0, iy0 = bx + 1, by + 1
        ix1, iy1 = bx + BLOCK - 2, by + BLOCK - 2
        self.fill(ix0, iy0, ix1, iy1, "roof_grey")
        for y in range(iy0, iy1 + 1):
            for x in range(ix0, ix1 + 1):
                if (x - ix0) % 4 == 0 or (y - iy0) % 4 == 0:
                    self.set(x, y, "concrete" if False else "roof_grey")
        self.fill(ix0 + 1, iy0 + 1, ix0 + 3, iy0 + 3, "roof_teal")
        for i in range(3):
            px, py = self.px(ix1 - 1 - i * 2, iy1 - 2)
            self.car_spawns.append((px, py, 192, 5))
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_IND)

    def p_water(self, bx, by):
        self.fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, "water")
        self.zone_fill(bx, by, bx + BLOCK - 1, by + BLOCK - 1, ZONE_DOCKS)

    PAINTERS = {
        "tower": p_tower, "houses": p_houses, "park": p_park,
        "parking": p_parking, "plaza": p_plaza, "mall": p_mall,
        "beach": p_beach, "dock": p_dock, "airport": p_airport,
        "water": p_water,
    }

    # ---- build ---------------------------------------------------------
    def build(self):
        self.roads()
        for cy in range(NC):
            for cx in range(NC):
                kind = LAYOUT[cy][cx]
                bx, by = cx * CELL + ROAD, cy * CELL + ROAD
                self.PAINTERS[kind](self, bx, by)

        # --- ambient traffic: cars on roads, away from intersections
        for i in range(64):
            cx, cy = self.rng.randrange(NC), self.rng.randrange(NC)
            x = cx * CELL + self.rng.choice([0, 3]) + (1 if self.rng.random() < 0.5 else 0)
            y = cy * CELL + ROAD + self.rng.randrange(BLOCK)
            if self.get(x, y) in (T["asphalt"], T["asphalt_v"], T["crosswalk"]):
                px, py = self.px(x, y)
                self.car_spawns.append((px, py, 192 if self.rng.random() < 0.5 else 0,
                                        self.rng.randrange(len(genart.CARS) - 1)))
        for i in range(48):
            cx, cy = self.rng.randrange(NC), self.rng.randrange(NC)
            x = cx * CELL + ROAD + self.rng.randrange(BLOCK)
            y = cy * CELL + self.rng.choice([0, 3])
            if self.get(x, y) in (T["asphalt"], T["asphalt_h"]):
                px, py = self.px(x, y)
                self.car_spawns.append((px, py, 64 if self.rng.random() < 0.5 else 128,
                                        self.rng.randrange(len(genart.CARS) - 1)))

        # --- pedestrians on sidewalks
        for i in range(120):
            cx, cy = self.rng.randrange(NC), self.rng.randrange(NC)
            x = cx * CELL + ROAD + self.rng.randrange(BLOCK)
            y = cy * CELL + ROAD + self.rng.randrange(BLOCK)
            if self.get(x, y) in (T["sidewalk"], T["curb_n"], T["curb_s"],
                                  T["curb_e"], T["curb_w"]):
                px, py = self.px(x, y)
                self.ped_spawns.append((px, py))

        # --- pickups scattered where a person would look for money
        spots = [(T["alley"], "bag"), (T["sidewalk"], "health"),
                 (T["parking"], "armour"), (T["grass"], "bag"),
                 (T["dock"], "ammo"), (T["sand"], "health")]
        for i in range(26):
            want, kind = spots[i % len(spots)]
            for _ in range(400):
                x = self.rng.randrange(W)
                y = self.rng.randrange(H)
                if self.get(x, y) == want:
                    px, py = self.px(x, y)
                    self.pickups.append((px, py, kind))
                    break

    # ---- queries -------------------------------------------------------
    def walkable(self, tx, ty):
        if not (0 <= tx < W and 0 <= ty < H):
            return False
        name = [n for n, i in T.items() if i == self.get(tx, ty)][0]
        return name not in SOLID and name not in WATER

    def flood(self, sx, sy):
        seen = bytearray(W * H)
        stack = [(sx, sy)]
        seen[sy * W + sx] = 1
        n = 0
        while stack:
            x, y = stack.pop()
            n += 1
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and not seen[ny * W + nx] \
                        and self.walkable(nx, ny):
                    seen[ny * W + nx] = 1
                    stack.append((nx, ny))
        return seen, n

    def nearest_walkable(self, tx, ty, maxr=10):
        if self.walkable(tx, ty):
            return tx, ty
        for r in range(1, maxr + 1):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    if self.walkable(tx + dx, ty + dy):
                        return tx + dx, ty + dy
        return tx, ty

    def nearest_reachable(self, tx, ty, seen, maxr=24):
        """Nearest tile that is not just walkable but actually reachable.

        A courtyard behind a mall is walkable and completely sealed; snapping a
        mission marker to it produces a mission nobody can finish.
        """
        if 0 <= tx < W and 0 <= ty < H and seen[ty * W + tx]:
            return tx, ty
        for r in range(1, maxr + 1):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    nx, ny = tx + dx, ty + dy
                    if 0 <= nx < W and 0 <= ny < H and seen[ny * W + nx]:
                        return nx, ny
        return tx, ty


# ---------------------------------------------------------------------------
# points of interest: missions reference these by name, so they cannot drift
POI_NAMES = [
    "SPAWN", "CLUB", "TOWER", "MALL", "PARK", "BEACH", "DOCKS", "AIRPORT",
    "GARAGE", "BRIDGE", "PROMENADE", "STADIUM",
]


def poi_tiles(city, seen):
    """Tile coordinates for each POI, snapped to ground you can actually reach."""
    c = lambda cx, cy: (cx * CELL + ROAD + BLOCK // 2,
                        cy * CELL + ROAD + BLOCK // 2)
    raw = {
        "SPAWN": (2 * CELL + 1, 0 * CELL + 8),      # on the road south of the club
        "CLUB": c(2, 0),
        "TOWER": c(2, 2),
        "MALL": c(3, 2),
        "PARK": c(1, 1),
        "BEACH": (5 * CELL + 8, 2 * CELL + ROAD + 3),
        "DOCKS": c(2, 5),
        "AIRPORT": c(0, 5),
        "GARAGE": (0 * CELL + ROAD + 3, 3 * CELL + ROAD + 8),
        "BRIDGE": (4 * CELL + 1, 4 * CELL + ROAD + 6),
        "PROMENADE": (5 * CELL + ROAD + 1, 1 * CELL + ROAD + 6),
        "STADIUM": (1 * CELL + ROAD + 6, 4 * CELL + ROAD + 2),
    }
    return {k: city.nearest_reachable(raw[k][0], raw[k][1], seen)
            for k in POI_NAMES}


# ---------------------------------------------------------------------------
def render_preview(city, path):
    """Whole city, 1:1, downscaled. Cheap way to notice that a district is a
    wall of buildings with no way in."""
    tiles = {name: gentiles.BUILDERS[name]() for name in T}
    img = pal.Image.new("RGB", (W * 16, H * 16))
    rgb = pal.rgb24()
    px = img.load()
    for ty in range(H):
        for tx in range(W):
            cv = tiles[[n for n, i in T.items() if i == city.get(tx, ty)][0]]
            for y in range(16):
                for x in range(16):
                    c = cv.px[y * 16 + x]
                    px[tx * 16 + x, ty * 16 + y] = (rgb[c * 3], rgb[c * 3 + 1],
                                                    rgb[c * 3 + 2])
    img = img.resize((W * 4, H * 4), pal.Image.NEAREST)
    img.save(path)


def main():
    os.makedirs(GEN, exist_ok=True)
    city = City()
    city.build()

    # spawn first, then flood from it, then snap every POI to reachable ground
    sx, sy = city.nearest_walkable(2 * CELL + 1, 8)
    seen, reached = city.flood(sx, sy)
    poi = poi_tiles(city, seen)
    total_walk = sum(1 for y in range(H) for x in range(W) if city.walkable(x, y))

    bad = []
    for name, (tx, ty) in poi.items():
        if not seen[ty * W + tx]:
            bad.append(name)
    if bad:
        raise SystemExit("unreachable POI on foot: %s" % ", ".join(bad))
    if reached < total_walk * 0.97:
        print("warning: only %d of %d walkable tiles reachable from spawn"
              % (reached, total_walk))

    # ---- emit C
    with open(os.path.join(GEN, "city.h"), "w", encoding="ascii") as f:
        f.write("/* generated by tools/gencity.py - do not edit */\n")
        f.write("#ifndef GTA_CITY_H\n#define GTA_CITY_H\n\n")
        f.write("#define CITY_W %d\n#define CITY_H %d\n" % (W, H))
        f.write("#define CITY_PX_W %d\n#define CITY_PX_H %d\n" % (W * 16, H * 16))
        f.write("#define N_CAR_SPAWNS %d\n" % len(city.car_spawns))
        f.write("#define N_PED_SPAWNS %d\n" % len(city.ped_spawns))
        f.write("#define N_PICKUPS %d\n" % len(city.pickups))
        f.write("#define N_PROPS %d\n" % len(city.props))
        f.write("#define N_POI %d\n\n" % len(POI_NAMES))
        f.write("enum {\n")
        for i, name in enumerate(POI_NAMES):
            f.write("  POI_%s = %d,\n" % (name, i))
        f.write("};\n\n")
        f.write("enum { Z_DOWNTOWN, Z_SUBURB, Z_BEACH, Z_DOCKS, Z_PARK, Z_IND };\n\n")
        f.write("typedef struct { short x, y; } Point;\n")
        f.write("typedef struct { short x, y; unsigned char angle, model; } CarSpawn;\n")
        f.write("typedef struct { short x, y; unsigned char kind; } PickupSpawn;\n")
        f.write("typedef struct { short x, y; unsigned char spr; } Prop;\n\n")
        f.write("extern const unsigned char g_map[CITY_W * CITY_H];\n")
        f.write("extern const unsigned char g_zone[CITY_W * CITY_H];\n")
        f.write("extern const Point g_poi[N_POI];\n")
        f.write("extern const CarSpawn g_car_spawns[N_CAR_SPAWNS];\n")
        f.write("extern const Point g_ped_spawns[N_PED_SPAWNS];\n")
        f.write("extern const PickupSpawn g_pickup_spawns[N_PICKUPS];\n")
        f.write("extern const Prop g_props[N_PROPS];\n\n")
        f.write("#endif\n")

    with open(os.path.join(GEN, "city.c"), "w", encoding="ascii") as f:
        f.write("/* generated by tools/gencity.py - do not edit */\n")
        f.write("#include \"city.h\"\n\n")
        f.write("const unsigned char g_map[CITY_W * CITY_H] = {\n")
        for y in range(H):
            row = city.t[y * W:(y + 1) * W]
            f.write("  " + ",".join(str(b) for b in row) + ",\n")
        f.write("};\n\n")
        f.write("const unsigned char g_zone[CITY_W * CITY_H] = {\n")
        for y in range(H):
            row = city.zone[y * W:(y + 1) * W]
            f.write("  " + ",".join(str(b) for b in row) + ",\n")
        f.write("};\n\n")

        def points(name, ctype, items, fmt):
            f.write("const %s %s[%d] = {\n" % (ctype, name, len(items)))
            for it in items:
                f.write("  " + fmt % it + ",\n")
            f.write("};\n\n")

        poi_px = [(poi[n][0] * 16 + 8, poi[n][1] * 16 + 8) for n in POI_NAMES]
        points("g_poi", "Point", poi_px, "{%d,%d}")
        points("g_car_spawns", "CarSpawn", city.car_spawns, "{%d,%d,%d,%d}")
        points("g_ped_spawns", "Point", city.ped_spawns, "{%d,%d}")

        kinds = {"bag": 0, "case": 1, "health": 2, "armour": 3, "ammo": 4}
        f.write("const PickupSpawn g_pickup_spawns[N_PICKUPS] = {\n")
        for x, y, k in city.pickups:
            f.write("  {%d,%d,%d},\n" % (x, y, kinds[k]))
        f.write("};\n\n")
        f.write("const Prop g_props[N_PROPS] = {\n")
        for x, y, s in city.props:
            f.write("  {%d,%d,%d},\n" % (x, y, s))
        f.write("};\n")

    render_preview(city, os.path.join(ROOT, "build", "preview_city.png"))
    print("wrote city.c/h: %d props, %d car spawns, %d peds, %d pickups, "
          "%d/%d tiles reachable"
          % (len(city.props), len(city.car_spawns), len(city.ped_spawns),
             len(city.pickups), reached, total_walk))


if __name__ == "__main__":
    main()

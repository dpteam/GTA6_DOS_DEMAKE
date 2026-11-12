"""Print the generated city as ASCII, with a ruler. For when a district looks
wrong on the map and you need to know which tile is the wall.

  python tools\\showmap.py            # whole city, 2 chars per tile
  python tools\\showmap.py 40 30 60 60  # a window: x0 y0 x1 y1
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CITY_C = os.path.join(ROOT, "build", "gen", "city.c")

sys.path.insert(0, os.path.join(ROOT, "tools"))
import gentiles  # noqa: E402

SYMBOLS = {
    "void": "  ", "asphalt": "..", "asphalt_v": "|.", "asphalt_h": "--",
    "crosswalk": "##", "manhole": "oo", "sidewalk": "::", "curb_n": "^^",
    "curb_s": "vv", "curb_e": ">>", "curb_w": "<<", "grass": ",,", "grass2": ";;",
    "sand": "~~", "water": "WW", "water2": "ww", "parking": "PP",
    "roof_grey": "RR", "roof_red": "rr", "roof_teal": "TT",
    "roof_glass": "GG", "roof_brown": "BB", "dock": "DD", "plaza": "++",
    "dirt": "dd", "alley": "aa",
}


def load():
    src = open(CITY_C, encoding="ascii").read()
    m = re.search(r"g_map\[CITY_W \* CITY_H\] = \{(.*?)\n\};", src, re.S)
    rows = []
    for line in m.group(1).strip().splitlines():
        rows.append([int(v) for v in line.strip().strip(",").split(",")])
    return rows


def main():
    rows = load()
    names = [n for n, _ in gentiles.TILES]
    if len(sys.argv) >= 5:
        x0, y0, x1, y1 = (int(v) for v in sys.argv[1:5])
    else:
        x0, y0, x1, y1 = 0, 0, len(rows[0]) - 1, len(rows) - 1

    print("     " + "".join("%-2d" % (x // 10 % 10) for x in range(x0, x1 + 1)))
    print("     " + "".join("%-2d" % (x % 10) for x in range(x0, x1 + 1)))
    for y in range(y0, y1 + 1):
        line = "".join(SYMBOLS.get(names[rows[y][x]], "??")
                       for x in range(x0, x1 + 1))
        print("%4d %s" % (y, line))


if __name__ == "__main__":
    main()

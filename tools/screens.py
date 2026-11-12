"""Render every screen of the game to docs/screens/*.png, in one go.

Boots the host build in tour mode, which walks the state machine and dumps one
frame per screen. Reviewing a menu by booting DOSBox and pressing keys is how
you ship a typo.

Output goes to docs/screens/ rather than build/ because these images are
committed: the README shows them, and a README with six broken image links is
worse than no README.
"""
import glob
import os
import shutil
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRAMES = os.path.join(ROOT, "build", "frames")
OUT = os.path.join(ROOT, "docs", "screens")

NAMES = ["title", "intro", "play", "pause", "map", "radio", "controls",
         "about", "wasted", "busted", "missiondone", "credits"]


def main():
    exe = os.path.join(ROOT, "build", "gta6_host.exe")
    if not os.path.exists(exe):
        raise SystemExit("build first: python tools/build.py host")

    for d in (FRAMES, OUT):
        if os.path.isdir(d):
            shutil.rmtree(d)
        os.makedirs(d)

    env = dict(os.environ)
    env["GTA_TOUR"] = "1"
    env["GTA_DUMP_EVERY"] = "20"
    env["GTA_MAX_FRAMES"] = "0"
    subprocess.run([exe], cwd=ROOT, env=env, check=True,
                   stdout=subprocess.DEVNULL)

    files = sorted(glob.glob(os.path.join(FRAMES, "*.ppm")))
    for i, f in enumerate(files):
        name = NAMES[i] if i < len(NAMES) else "screen%02d" % i
        im = Image.open(f)
        im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(
            os.path.join(OUT, "%02d_%s.png" % (i, name)))
    print("wrote %d screens to docs/screens/" % len(files))


if __name__ == "__main__":
    main()

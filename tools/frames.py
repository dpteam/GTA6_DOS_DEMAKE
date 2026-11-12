"""Contact sheet of the host build's PPM frames, for eyeballing a run.

  python tools\\frames.py                 build/frames/f*.ppm -> tools/prev_frames.png
  python tools\\frames.py "f*.ppm" out.png 4 2 12
"""
import glob
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRAMES = os.path.join(ROOT, "build", "frames")


def main(pattern="f*.ppm", out="tools/prev_frames.png", cols=4, scale=1,
         limit=16):
    files = sorted(glob.glob(os.path.join(FRAMES, pattern)))[:limit]
    if not files:
        print("no frames in", FRAMES)
        return
    ims = [Image.open(f) for f in files]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * h), (0, 0, 0))
    for i, im in enumerate(ims):
        sheet.paste(im, ((i % cols) * w, (i // cols) * h))
    if scale != 1:
        sheet = sheet.resize((sheet.width * scale, sheet.height * scale),
                             Image.NEAREST)
    path = os.path.join(ROOT, out)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)
    print("wrote", out, sheet.size, "from", len(files), "frames")


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0] if len(a) > 0 else "f*.ppm",
         a[1] if len(a) > 1 else "tools/prev_frames.png",
         int(a[2]) if len(a) > 2 else 4,
         int(a[3]) if len(a) > 3 else 1,
         int(a[4]) if len(a) > 4 else 16)

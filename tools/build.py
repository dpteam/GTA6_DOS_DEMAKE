"""Build GTA VI: DOS CITY.

  python tools/build.py host   -> native binary for debugging (dumps PPM frames)
  python tools/build.py dos    -> real DJGPP .EXE for DOSBox

Every generated file lives in build/gen and is rebuilt every time, because
stale generated assets are how you end up debugging a sprite you deleted.
"""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vendor  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")
OBJ = os.path.join(ROOT, "build", "obj")
DIST = os.path.join(ROOT, "dist")
FRAMES = os.path.join(ROOT, "build", "frames")

SOURCES = ["video.c", "render.c", "sound.c", "g_world.c", "g_play.c",
           "g_mission.c", "g_ui.c", "game.c", "main.c"]
GEN_SOURCES = ["paldata.c", "assets.c", "tiles.c", "font.c", "music.c",
               "city.c"]

GENERATORS = ["genpal.py", "gentiles.py", "genart.py", "genfont.py",
              "genmusic.py", "genstrings.py", "gencity.py"]

def run(cmd, **kw):
    print("  " + (cmd if isinstance(cmd, str) else " ".join(cmd)))
    r = subprocess.run(cmd, **kw)
    if r.returncode != 0:
        raise SystemExit("build failed (%d)" % r.returncode)
    return r


def gen_assets():
    for script in GENERATORS:
        print("[gen] " + script)
        run([sys.executable, os.path.join(ROOT, "tools", script)], cwd=ROOT,
            stdout=subprocess.DEVNULL)


def djgpp_env():
    dj = vendor.djgpp_dir()
    env = dict(os.environ)
    env["PATH"] = (os.path.join(dj, "i586-pc-msdosdjgpp", "bin") + ";" +
                   os.path.join(dj, "bin") + ";" + env["PATH"])
    env["GCC_EXEC_PREFIX"] = os.path.join(dj, "lib", "gcc") + os.sep
    env["DJDIR"] = os.path.join(dj, "i586-pc-msdosdjgpp")
    return env


def build(kind):
    os.makedirs(OBJ, exist_ok=True)
    os.makedirs(FRAMES, exist_ok=True)
    gen_assets()

    env = None
    if kind == "host":
        cc = ["gcc", "-O2", "-Wall", "-Wno-unused-parameter", "-Wno-unused-function",
              "-DGTA_HOST", "-Isrc", "-I" + GEN]
        exe = os.path.join(ROOT, "build", "gta6_host.exe")
    else:
        env = djgpp_env()
        cc = [os.path.join(vendor.djgpp_dir(), "bin",
                           "i586-pc-msdosdjgpp-gcc.exe"),
              "-O2", "-Wall", "-Wno-unused-parameter", "-Wno-unused-function",
              "-fno-strict-aliasing", "-Isrc", "-I" + GEN]
        exe = os.path.join(DIST, "GTA6DOS.EXE")
        os.makedirs(DIST, exist_ok=True)

    objs = []
    for src in SOURCES + GEN_SOURCES:
        base = os.path.splitext(src)[0]
        path = os.path.join(ROOT, "src", src)
        if not os.path.exists(path):
            path = os.path.join(GEN, src)
        obj = os.path.join(OBJ, "%s.%s.o" % (base, kind))
        run(cc + ["-c", path, "-o", obj], env=env)
        objs.append(obj)

    run(cc + objs + ["-o", exe], env=env)
    print("built:", exe, os.path.getsize(exe), "bytes")

    if kind == "dos":
        src = vendor.cwsdpmi()
        if src:
            shutil.copy2(src, os.path.join(DIST, "CWSDPMI.EXE"))
            print("copied CWSDPMI.EXE")
        else:
            print("WARNING: CWSDPMI.EXE not found, run tools/setup.py")
    return exe


def gen_icon():
    """Banner and repo icon. Separate from gen_assets because it needs Pillow
    and the generated font, and neither is required to build the EXE."""
    try:
        import PIL  # noqa: F401
    except ImportError:
        print("[gen] genicon.py skipped (Pillow not installed)")
        return
    if not os.path.exists(os.path.join(GEN, "font.c")):
        return
    print("[gen] genicon.py")
    run([sys.executable, os.path.join(ROOT, "tools", "genicon.py")], cwd=ROOT,
        stdout=subprocess.DEVNULL)


def main():
    args = sys.argv[1:]
    if args and args[0] in ("icon", "assets"):
        gen_assets()
        gen_icon()
        return
    build(args[0] if args else "host")
    if not args or args[0] == "dos":
        gen_icon()


if __name__ == "__main__":
    main()

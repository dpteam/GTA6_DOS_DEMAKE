"""Locate the vendor bits (DJGPP, CWSDPMI, Unifont).

The demake is meant to be buildable on a machine where the neighbouring
`DOSYA` project already ran `tools/setup.py`: its vendor/ folder is used
instead of downloading 130 MB again. Set GTA_VENDOR to override.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIBLING = os.path.join(os.path.dirname(ROOT), "DOSYA", "vendor")


def _first(*paths):
    for p in paths:
        if p and os.path.exists(p):
            return p
    return None


def vendor_dir():
    env = os.environ.get("GTA_VENDOR")
    local = os.path.join(ROOT, "vendor")
    if env and os.path.isdir(env):
        return env
    if os.path.isdir(os.path.join(local, "djgpp")):
        return local
    if os.path.isdir(os.path.join(SIBLING, "djgpp")):
        return SIBLING
    return local


def djgpp_dir():
    return os.path.join(vendor_dir(), "djgpp", "djgpp")


def unifont():
    return _first(os.path.join(vendor_dir(), "unifont.hex.gz"),
                  os.path.join(SIBLING, "unifont.hex.gz"))


def cwsdpmi():
    return _first(os.path.join(vendor_dir(), "csdpmi", "bin", "CWSDPMI.EXE"),
                  os.path.join(SIBLING, "csdpmi", "bin", "CWSDPMI.EXE"))

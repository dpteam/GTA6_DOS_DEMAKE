"""Sanity-check the generated MIDI files.

A .MID that a player refuses to open is a silent failure: the game still
builds, and nobody notices the reference mix is broken until someone tries to
listen to it. This parses the header and the track chunk length, which is
exactly what a player checks first.

  python tools/check_midi.py            # checks dist/RADIO/*.MID
  python tools/check_midi.py DIR        # checks another directory
"""
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXPECTED = 6


def check(path):
    data = open(path, "rb").read()
    if len(data) < 22:
        return "too short (%d bytes)" % len(data)
    if data[:4] != b"MThd":
        return "missing MThd"
    hlen, fmt, ntr, div = struct.unpack(">IHHH", data[4:14])
    if hlen != 6:
        return "header length %d, expected 6" % hlen
    if fmt != 0:
        return "format %d, expected 0 (single track)" % fmt
    if ntr != 1:
        return "%d tracks, expected 1" % ntr
    if div != 96:
        return "division %d, expected 96 PPQ" % div
    if data[14:18] != b"MTrk":
        return "missing MTrk"
    tlen = struct.unpack(">I", data[18:22])[0]
    if 22 + tlen != len(data):
        return "track length %d does not match file size %d" % (tlen, len(data))
    if data[-3:] != b"\xff\x2f\x00":
        return "missing end-of-track event"
    return None


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        ROOT, "dist", "RADIO")
    files = sorted(glob.glob(os.path.join(directory, "*.MID")))
    if not files:
        raise SystemExit("no .MID files in %s" % directory)
    if len(files) != EXPECTED:
        raise SystemExit("expected %d MIDI files, found %d"
                         % (EXPECTED, len(files)))

    bad = 0
    for f in files:
        err = check(f)
        name = os.path.basename(f)
        if err:
            print("FAIL %-14s %s" % (name, err))
            bad += 1
        else:
            print("ok   %-14s %d bytes" % (name, os.path.getsize(f)))

    if bad:
        raise SystemExit("%d of %d MIDI files are broken" % (bad, len(files)))
    print("\nall %d MIDI files are valid" % len(files))


if __name__ == "__main__":
    main()

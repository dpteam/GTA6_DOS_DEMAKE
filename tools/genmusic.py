"""Radio stations -> build/gen/music.c + dist/RADIO/*.MID

One source of truth, two outputs:

  * a Standard MIDI File you can play in Winamp and judge me with;
  * a byte table the game feeds to a 1-bit PC speaker at 1193182 Hz.

The songs are written in degree notation so they stay readable:

    "0.3.5.7"  -> scale degrees above the bar's root, '.' = rest, '=' = hold

Each bar carries a root offset, so a four-bar loop over i-VI-III-VII writes
itself. Bass is the same notation one or two octaves down. Drums are one bar
of 'k'/'s'/'h'/'.' repeated.

The MIDI file and the speaker table are generated from the same events, so if
the game sounds wrong, the .MID is the reference mix.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pal  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "build", "gen")
DIST_MIDI = os.path.join(ROOT, "dist", "RADIO")

# ---------------------------------------------------------------- notation ---
NOTE_BASE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6,
             "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}

SCALES = {
    "minor":      [0, 2, 3, 5, 7, 8, 10, 12, 14, 15],
    "major":      [0, 2, 4, 5, 7, 9, 11, 12, 14, 16],
    "dorian":     [0, 2, 3, 5, 7, 9, 10, 12, 14, 15],
    "phrygian":   [0, 1, 3, 5, 7, 8, 10, 12, 13, 15],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10, 12, 14, 16],
    "harmminor":  [0, 2, 3, 5, 7, 8, 11, 12, 14, 15],
}


def note(name):
    """'A#3' -> MIDI number."""
    if len(name) > 2 and name[1] == "#":
        pitch, octave = name[:2], int(name[2:])
    else:
        pitch, octave = name[0], int(name[1:])
    return 12 * (octave + 1) + NOTE_BASE[pitch]


def expand(pattern, root, scale, octave_shift=0):
    """Degree pattern -> list of (midi note or 0, is_hold).

    A hold is kept distinct from the note it holds so the C side can sustain
    one note across steps while the MIDI side can emit one long note.
    """
    out = []
    last = 0
    for ch in pattern:
        if ch == ".":
            out.append((0, False))
            last = 0
        elif ch == "=":
            out.append((last, True))
        else:
            deg = int(ch)
            n = root + scale[deg % len(scale)] + 12 * (deg // len(scale))
            n += 12 * octave_shift
            n = max(0, min(127, n))
            out.append((n, False))
            last = n
    return out


class Song:
    def __init__(self, key, title, subtitle, bpm, wave, root, scale,
                 bars, lead, bass, drums, swing=0):
        self.key = key
        self.title = title
        self.subtitle = subtitle
        self.bpm = bpm
        self.wave = wave
        self.root = root
        self.scale = SCALES[scale]
        self.bars = bars            # list of root offsets in semitones
        self.lead_src = lead        # one 16-step string per bar
        self.bass_src = bass
        self.drums = drums
        self.swing = swing
        self.lead, self.bass = self.render()

    def render(self):
        lead, bass = [], []
        for i, off in enumerate(self.bars):
            root = self.root + off
            lead += expand(self.lead_src[i], root, self.scale)
            bass += expand(self.bass_src[i], root - 12, self.scale)
        return lead, bass


# ------------------------------------------------------------------ songs ----
# The joke is that each station is a real, distinct piece of music, not four
# beeps in a different order. A 1-bit speaker with good patterns sounds like
# a 1986 arcade board, and nobody ever complained about those.

SONGS = [
    Song(
        key="VICE", title="RADIO VICE SHORES 87.6",
        subtitle="synthwave dlya tekh, kto nikuda ne edet",
        bpm=100, wave="saw", root=note("A2"), scale="minor",
        bars=[0, -3, 5, -2],
        lead=["5.7.8.7.5.3.2.3.", "3.5.7.5.3.2.0.2.",
              "8.9.8.7.5.7.5.3.", "7.5.3.5.2.3.2.0."],
        bass=["0=0=0=3=", "0=0=0=2=", "0=0=0=3=", "0=0=0=2="],
        drums="k.h.s.h.k.hks.h.",
    ),
    Song(
        key="FLORIDA", title="FLORIDA-FM 101.1",
        subtitle="tvoyu devushku uzhe priglasili",
        bpm=126, wave="pulse", root=note("C3"), scale="major",
        bars=[0, 5, 9, 7],
        lead=["4.4.7.4.9.7.4...", "5.5.9.5.7.5.2...",
              "7.7.9.7.11.9.7..", "5.4.2.4.5.7....."],
        bass=["0=0=0=0=", "0=0=0=0=", "0=0=0=0=", "0=0=0=0="],
        drums="k.hsk.hsk.hsk.hs",
    ),
    Song(
        key="GATOR", title="ALLIGATOR COUNTRY 94.3",
        subtitle="dlya tekh, kto lyubit shtany i boltota",
        bpm=92, wave="square", root=note("E2"), scale="mixolydian",
        bars=[0, 0, 7, 5],
        lead=["0.0.3.0.5.3.0...", "0.0.3.0.7.5.3...",
              "7.7.9.7.5.3.0...", "5.5.7.5.3.0......"],
        bass=["0===0===0===0=", "0===0===0===0=",
              "0===0===0===0=", "0===0===0===0="],
        drums="k..h.s..k.h.s...",
    ),
    Song(
        key="TAXI", title="TAXI LATINO 105.7",
        subtitle="voditel znaet dorogu. i vash adres",
        bpm=134, wave="tri", root=note("D3"), scale="phrygian",
        bars=[0, 1, 0, -2],
        lead=["0.1.0.4.0.1.3...", "1.2.1.5.1.2.4...",
              "0.1.0.4.0.1.3...", "3.2.1.0.4.1.0..."],
        bass=["0.0.0.0.0.0.0.0.", "0.0.0.0.0.0.0.0.",
              "0.0.0.0.0.0.0.0.", "0.0.0.0.0.0.0.0."],
        drums="k.hks.hkk.hks.hk",
    ),
    Song(
        key="TISHINA", title="RADIO TISHINA 66.6",
        subtitle="zdes nichego net. kak i vezde",
        bpm=60, wave="noise", root=note("A1"), scale="minor",
        bars=[0, 0, 0, 0],
        lead=["0...............", "0...............",
              "0.......0.......", "0..............."],
        bass=["0...............", "0...............",
              "0...............", "0..............."],
        drums="................",
    ),
    Song(
        key="THEME", title="DOS CITY (main theme)",
        subtitle="",
        bpm=88, wave="saw", root=note("D2"), scale="harmminor",
        bars=[0, 0, 5, 3],
        lead=["0.0.5.0.8.7.5.3.", "2.0.5.0.7.5.3.2.",
              "8.7.5.3.5.7.8.9.", "7.5.3.2.0........"],
        bass=["0===0===3===2===", "0===0===3===2===",
              "0===0===3===2===", "0===0===3===2==="],
        drums="k..h..s.k..h.s..",
    ),
]

WAVE_ID = {"square": 0, "pulse": 1, "saw": 2, "tri": 3, "noise": 4}
DRUM_ID = {"k": 1, "s": 2, "h": 3}


# ------------------------------------------------------------------- MIDI ----
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.insert(0, (n & 0x7F) | 0x80)
        n >>= 7
    return bytes(out)


def midi_file(song, path):
    """Format 0, three tracks' worth of events on one track. Good enough."""
    step_ms = 60000.0 / song.bpm / 4.0
    ppq = 96
    step_ticks = ppq // 4
    events = []          # (tick, order, bytes)

    def emit(track, channel, seq, program_vel):
        """seq is a list of (note, is_hold); holds extend the previous note."""
        i = 0
        while i < len(seq):
            n, hold = seq[i]
            if n <= 0 or hold:
                i += 1
                continue
            j = i + 1
            while j < len(seq) and seq[j][1]:
                j += 1
            events.append((i * step_ticks, 1, bytes([0x90 | channel, n, program_vel])))
            events.append((max(i * step_ticks + 1, j * step_ticks - 1), 0,
                           bytes([0x80 | channel, n, 0])))
            i = j

    emit("lead", 0, song.lead, 88)
    emit("bass", 1, song.bass, 76)

    for i in range(len(song.lead)):
        ch = song.drums[i % 16]
        if ch in DRUM_ID:
            d = {"k": (36, 110), "s": (38, 96), "h": (42, 70)}[ch]
            events.append((i * step_ticks, 3, bytes([0x99, d[0], d[1]])))
            events.append((i * step_ticks + 2, 0, bytes([0x89, d[0], 0])))

    events.sort(key=lambda e: (e[0], e[1]))
    track = bytearray()
    tempo = int(60000000 / song.bpm)
    track += vlq(0) + bytes([0xFF, 0x51, 0x03]) + struct.pack(">I", tempo)[1:]
    track += vlq(0) + bytes([0xFF, 0x03]) + vlq(len(song.title)) + \
        song.title.encode("ascii")
    track += vlq(0) + bytes([0xFF, 0x58, 0x04, 4, 2, 24, 8])
    # program changes: square lead, synth bass
    track += vlq(0) + bytes([0xC0, 80])
    track += vlq(0) + bytes([0xC1, 38])

    last = 0
    for tick, _, data in events:
        track += vlq(max(0, tick - last)) + data
        last = tick
    track += vlq(0) + bytes([0xFF, 0x2F, 0x00])

    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


# ---------------------------------------------------------------------- C ----
def note_table():
    """MIDI note -> PIT divisor, computed here so the C side needs no floats."""
    divs = []
    for n in range(128):
        hz = 440.0 * (2.0 ** ((n - 69) / 12.0))
        divs.append(max(1, min(65535, int(1193182.0 / hz))))
    return divs


def main():
    os.makedirs(GEN, exist_ok=True)
    os.makedirs(DIST_MIDI, exist_ok=True)

    for s in SONGS:
        midi_file(s, os.path.join(DIST_MIDI, "%s.MID" % s.key))

    h = ["/* generated by tools/genmusic.py - do not edit */",
         "#ifndef GTA_MUSIC_H",
         "#define GTA_MUSIC_H",
         "",
         "#define N_SONGS %d" % len(SONGS),
         "#define SONG_STEPS %d" % len(SONGS[0].lead),
         "",
         "enum { W_SQUARE, W_PULSE, W_SAW, W_TRI, W_NOISE, W_NOISE_LO };",
         "",
         "typedef struct {",
         "  const char *title;",
         "  const char *subtitle;",
         "  unsigned char bpm, wave, steps;",
         "  const unsigned char *lead;   /* 0 rest, 255 hold */",
         "  const unsigned char *bass;",
         "  const char *drums;           /* 16 chars: k s h . */",
         "} Song;",
         "",
         "extern const Song g_songs[N_SONGS];",
         "extern const unsigned short g_note_div[128];",
         "",
         "#endif"]
    with open(os.path.join(GEN, "music.h"), "w", encoding="ascii") as f:
        f.write("\n".join(h) + "\n")

    with open(os.path.join(GEN, "music.c"), "w", encoding="ascii") as f:
        f.write("/* generated by tools/genmusic.py - do not edit */\n")
        f.write("#include \"music.h\"\n\n")
        for s in SONGS:
            for name, track in (("lead", s.lead), ("bass", s.bass)):
                vals = []
                for n, hold in track:
                    vals.append(255 if hold else max(0, n))
                f.write("static const unsigned char %s_%s[%d] = {\n  "
                        % (s.key.lower(), name, len(vals)))
                f.write(",".join(str(v) for v in vals))
                f.write("\n};\n")
        f.write("\nconst Song g_songs[N_SONGS] = {\n")
        for s in SONGS:
            f.write('  {"%s", "%s", %d, %d, %d, %s_lead, %s_bass, "%s"},\n'
                    % (s.title, s.subtitle, s.bpm, WAVE_ID[s.wave],
                       len(s.lead), s.key.lower(), s.key.lower(), s.drums))
        f.write("};\n\n")

        divs = note_table()
        f.write("/* MIDI note -> PIT channel-2 divisor */\n")
        f.write("const unsigned short g_note_div[128] = {\n")
        for i in range(0, 128, 8):
            f.write("  " + ",".join("%d" % d for d in divs[i:i + 8]) + ",\n")
        f.write("};\n")

    print("wrote music.c/h (%d songs) and %d .MID files"
          % (len(SONGS), len(SONGS)))


if __name__ == "__main__":
    main()

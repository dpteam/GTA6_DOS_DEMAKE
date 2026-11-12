---
name: Feature or content idea
about: A mission, a station, a district, a joke
title: "[idea] "
labels: enhancement
---

**What**

**Why it fits**

This is a 1994 game on a 1-bit speaker. Things that are cheap to add:

- a new mission (one entry in `g_missions[]` in `src/g_mission.c`)
- a new radio station (one `Song(...)` in `tools/genmusic.py`)
- a new district block (`gencity.py`, a painter plus a layout entry)
- a new tile or sprite (a small function in `tools/gentiles.py` /
  `tools/genart.py`)
- a new screen (`g_ui.c` plus a state in `game.c`)

**What it costs**

If it needs more than ~4 KB of ROM or a new subsystem, say so honestly and
we will argue about it. Politely.

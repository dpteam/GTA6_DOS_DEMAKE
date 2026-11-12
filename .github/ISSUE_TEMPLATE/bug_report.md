---
name: Bug report
about: Something in the game is broken, and it is not the 1994 aesthetic
title: "[bug] "
labels: bug
---

**What happened**

<!-- One sentence. "The car drove through a building" is a bug report.
     "Cars feel weird" is a mood. -->

**What you expected**

**How to reproduce**

1.
2.
3.

**Where**

- [ ] Host build (`python tools\build.py host`)
- [ ] DOS build in DOSBox (`python tools\build.py dos`)
- [ ] The autopilot found it (`GTA_AUTOPILOT=1`)

If it is in-game, which district and roughly which tile? The map tool helps:

```powershell
python tools\showmap.py 40 30 60 60
```

**Logs**

If it is the DOS build, `dist\RUNLOG.TXT` is written on exit:

```
frames=300 renders=302 ticks=4904
logic/s=61.2 render/s=61.6
state=2 mission=0 money=300 health=100
player tile=43,13
```

If `logic/s` and `render/s` disagree badly, that is the bug, not the symptom.

**Environment**

| | |
|---|---|
| OS | |
| DOSBox version / cycles | |
| Python version | |
| Commit | `git rev-parse --short HEAD` |

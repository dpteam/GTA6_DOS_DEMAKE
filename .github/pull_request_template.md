## What changed

<!-- One paragraph. If it needs three, it is two pull requests. -->

## Which part

- [ ] engine (`src/`)
- [ ] asset generators (`tools/gen*.py`)
- [ ] docs
- [ ] CI

## Checks

- [ ] `python tools\build.py dos` produces `dist\GTA6DOS.EXE`
- [ ] `python tools\build.py host` produces `build\gta6_host.exe`
- [ ] the autopilot still finishes the story:
      `GTA_AUTOPILOT=1 .\build\gta6_host.exe` → `story COMPLETE`
- [ ] if generators changed, `build/gen/` is still deterministic
      (run `python tools\build.py assets` twice and diff)
- [ ] if the city changed, `python tools\gencity.py` still reports
      `7563/7563 tiles reachable`

## Notes for the reviewer

<!-- Anything you tried that did not work. That is usually the useful part. -->

# Сборка и запуск

## Что нужно

| | |
|---|---|
| Python | 3.10+ (только стандартная библиотека, плюс Pillow для превью) |
| DJGPP | скачивается `tools/setup.py` |
| DOSBox | любой, для запуска |
| GCC | для host-сборки (MSVC/MinGW под Windows, gcc под Linux/macOS) |

Pillow нужен только для превью и иконок. Без него игра собирается и
работает, просто не будет `build/preview_*.png` и `docs/*.png`.

```powershell
pip install pillow
```

## Первая сборка

```powershell
python tools\setup.py
```

Скачивает:

* `djgpp-mingw-gcc1220-standalone.zip` (~130 МБ) — кросс-компилятор;
* `csdpmi7b.zip` — DPMI-хост, без него DOS-сборка падает с
  «no DPMI - Get csdpmi*b.zip» ещё до первого пикселя;
* `unifont-15.1.05.hex.gz` — источник шрифта.

Если рядом есть проект `DOSYA` с уже скачанным `vendor\`, `setup.py` возьмёт
его и не будет качать заново. Указать явно:

```powershell
$env:GTA_VENDOR="D:\djgpp-vendor"
```

## Сборка

```powershell
python tools\build.py dos      # dist\GTA6DOS.EXE
python tools\build.py host     # build\gta6_host.exe
python tools\build.py icon     # только баннер и иконку
python tools\build.py assets   # только сгенерированные ассеты
```

`build.py` каждый раз перегенерирует всё в `build/gen/`. Устаревшие
сгенерированные ассеты — это как раз то, из-за чего потом отлаживаешь
спрайт, который ты удалил.

## Запуск в DOSBox

```
mount c dist
c:
GTA6DOS.EXE
```

Или одной командой:

```powershell
dosbox -conf dosbox.conf
```

`dosbox.conf` уже настроен: 16 МБ, `svga_s3`, `cycles=auto`,
`nosound=true` (PC speaker DOSBox эмулирует, но в конфиге звук выключен,
чтобы не мешать; уберите `nosound=true`, если хотите слышать радио).

## Автотест

```powershell
# весь сюжет без человека
$env:GTA_AUTOPILOT="1"
.\build\gta6_host.exe

# 300 кадров в настоящей DOS-сборке, со скриншотами
dosbox -conf build\test.conf
```

Host-тест печатает итог:

```
=== autopilot summary ===
frames=38131 renders=38131
missions completed: 7/7
money=20810 state=9
story COMPLETE
```

DOS-тест пишет `dist\RUNLOG.TXT`:

```
frames=300 renders=302 ticks=4904
logic/s=61.2 render/s=61.6
state=2 mission=0 money=300 health=100
player tile=43,13
```

`logic/s` и `render/s` — главные числа. Если они расходятся сильно,
значит сломан лимитер кадров. Если `logic/s` около 60, а игра «летает»,
значит сломан таймер, а не игра.

## Решение проблем

**`no DPMI - Get csdpmi*b.zip`**
Нет `CWSDPMI.EXE` рядом с `.EXE`. `build.py dos` копирует его сам; если не
скопировал — `vendor/csdpmi/bin/CWSDPMI.EXE` отсутствует, запустите
`tools/setup.py`.

**Игра запускается и сразу выходит**
Проверьте `dist\RUNLOG.TXT`. Если файла нет — упало до `main()`, скорее
всего DPMI.

**Кракозябры вместо русского**
Значит шрифт собран не из CP866. Пересоберите: `python tools\genfont.py`.
Весь текст в C — CP866-байты, поэтому проблема всегда в шрифте.

**Чёрный экран, но RUNLOG пишется**
`vid_present()` не копирует буфер. В DOS это `memcpy` в `A000:0000`;
проверьте, что `vid_init()` вызвал `__djgpp_nearptr_enable()`.

**Звук не слышно**
`nosound=true` в `dosbox.conf`. Уберите.

**Всё тормозит**
Уменьшите `cycles` не надо — наоборот, поднимите. На `cycles=fixed 30000`
DOSBox эмулирует примерно 386, и рендер даёт 15 fps. `cycles=auto` даёт 60.

**`build/gen/` в странном состоянии**
Удалите и пересоберите. Генераторы идемпотентны, терять нечего:

```powershell
Remove-Item -Recurse -Force build\gen
python tools\build.py dos
```

## CI

`.github/workflows/ci.yml` делает три вещи:

1. **build** — ставит DJGPP, собирает `GTA6DOS.EXE`, проверяет размер
   и наличие `CWSDPMI.EXE`;
2. **story** — собирает host-версию и прогоняет автопилот, требуя
   `story COMPLETE`. Это единственный тест, который действительно важен;
3. **artifacts** — заливает `dist\` и скриншоты экранов как артефакты.

Тяжёлый шаг (скачивание 130 МБ DJGPP) кэшируется между запусками.

Локально то же самое:

```powershell
python tools\setup.py
python tools\build.py dos
$env:GTA_AUTOPILOT="1"; .\build\gta6_host.exe | Select-String "story"
```

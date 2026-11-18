# HomeBrew

Programme fuer die Station: **Script-Sprache** (`.hbs`), gebaut zu verschluesselten `.hb`.

## Wichtig

| Idee | Realitaet |
|------|-----------|
| C / C++ / Python auf der SD | Nein – ESP32 laedt keinen nativen Code von SD in dieser Firmware |
| Echtes MP4 abspielen | Nein – zu wenig RAM/CPU ohne Spezial-Port |
| Scripts mit Screen, Farben, Touch | Ja |
| `.hb` per GitHub Actions bauen + Release | Ja |

## Programmieren

Datei `homebrew/src/<name>/payload.hbs`:

```
CLEAR
COLOR ORANGE
CENTER 40 Mein Programm
TEXT 20 80 Hello
WAIT 1000
WAIT_TOUCH
```

Befehle: `CLEAR`, `COLOR`, `TEXT x y`, `CENTER y`, `WAIT ms`, `WAIT_TOUCH`, `BTN`

## Bauen

Lokal:
```bash
python tools/pack_homebrew.py --build
```

CI: bei jedem Push auf `main` werden alle `.hb` gebaut und im GitHub Release mitgeliefert (`homebrew-*.zip` + einzelne `.hb`).

## Auf die SD

`homebrew/out/*.hb` nach `/homebrew/` auf der SD kopieren.

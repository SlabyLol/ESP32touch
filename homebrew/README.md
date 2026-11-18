# HomeBrew

## Regel

1. Programmieren nur in **`.py`**, **`.cpp`** oder **`.cs`**
2. Builder macht daraus **`.hb`**
3. Nur **`.hb`** auf die SD / ins Release

```bash
python tools/pack_homebrew.py --build
```

Andere Endungen (`.txt`, `.hbs`, …) werden abgelehnt.

## Projekt

```
homebrew/src/mein_ding/
  info.json
  main.py      # oder main.cpp oder main.cs
```

`info.json`:
```json
{
  "title": "Mein Ding",
  "author": "Du",
  "version": "1.0",
  "description": "...",
  "payload": "main.py",
  "output": "mein_ding.hb"
}
```

## Befehle im Quellcode

In der `.py` / `.cpp` / `.cs` stehen die HomeBrew-Anzeige-Befehle:

`CLEAR` `COLOR` `TEXT` `CENTER` `WAIT` `WAIT_TOUCH`

(Das ist kein voller Python-/C++-/C#-Compiler auf dem ESP32 – die Datei wird verschluesselt nach `.hb` gepackt und die Befehle werden dort ausgefuehrt.)

## CI

GitHub Actions baut alle Projekte zu `.hb` und legt sie ins Release.

# HomeBrew Builder

Eigenes verschluesseltes Format **`.hb`** fuer ESP32touch.

## Struktur

```
homebrew/
  src/                  <- deine Projekte
    demo_hallo/
      info.json         <- Titel, Author, Version, Beschreibung
      payload.txt       <- Inhalt
    station_info/
    secret_note/
  out/                  <- fertige .hb nach dem Build
```

## Bauen

```bash
python tools/pack_homebrew.py --build
```

Erzeugt alle `.hb` Dateien nach `homebrew/out/`.

## Neues Projekt anlegen

1. Ordner unter `homebrew/src/mein_projekt/` erstellen
2. `info.json` + `payload.txt` (oder andere Payload-Datei) anlegen
3. `python tools/pack_homebrew.py --build`

### info.json Beispiel

```json
{
  "title": "Mein Homebrew",
  "author": "DeinName",
  "version": "1.0",
  "description": "Was es macht",
  "payload": "payload.txt",
  "output": "mein_homebrew.hb"
}
```

## Auf die SD

Inhalt von `homebrew/out/` nach:

- `/homebrew/` auf der SD
- oder Root `/`
- oder `/hb/`

Dann auf dem Display **HomeBrew** oeffnen.

## Einzeldatei ohne Projektordner

```bash
python tools/pack_homebrew.py "Titel" --text "Text hier" datei.hb
python tools/pack_homebrew.py "Titel" input.bin datei.hb
```

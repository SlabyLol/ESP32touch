# HomeBrew – programmieren fuer DarkFox Station

Eigene Programme als verschluesselte `.hb` Dateien. Keine extra Firmware.

## Schnellstart

```bash
python tools/pack_homebrew.py --build
```

Fertige Dateien liegen in `homebrew/out/`. Auf die SD nach `/homebrew/` kopieren.

## Neues Programm schreiben

1. Ordner anlegen: `homebrew/src/mein_spiel/`
2. `info.json` + `payload.hbs` (Script)
3. `python tools/pack_homebrew.py --build`

### info.json

```json
{
  "title": "Mein Spiel",
  "author": "Du",
  "version": "1.0",
  "description": "Kurze Beschreibung",
  "payload": "payload.hbs",
  "output": "mein_spiel.hb"
}
```

### Script-Sprache (payload.hbs)

| Befehl | Bedeutung |
|--------|-----------|
| `CLEAR` | Bildschirm schwarz |
| `COLOR ROT` | Farbe: RED GREEN BLUE WHITE YELLOW CYAN ORANGE MAGENTA NAVY oder Hex `FF8800` |
| `TEXT x y Text` | Text an Position |
| `CENTER y Text` | Text zentriert |
| `WAIT 1000` | Warten in ms (max 10000) |
| `WAIT_TOUCH` | Warten bis Touch |
| `BTN` | wie WAIT_TOUCH |
| `# Kommentar` | wird ignoriert |

### Beispiel

```
CLEAR
COLOR ORANGE
CENTER 40 Mein Homebrew
COLOR WHITE
TEXT 20 90 Zeile eins
WAIT 1000
COLOR GREEN
CENTER 160 Touch zum Ende
WAIT_TOUCH
```

## Struktur

```
homebrew/
  src/<projekt>/info.json
  src/<projekt>/payload.hbs
  out/*.hb
```

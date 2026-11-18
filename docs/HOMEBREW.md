# DarkFox HomeBrew Format (.hb)

Eigenes Dateiformat fuer die ESP32touch Station. Verschluesselt, eigener Name, eigener Aufbau.

## Dateiendung

`.hb`

## Speicherorte auf der SD

- `/`
- `/homebrew/`
- `/HomeBrew/`
- `/hb/`

## Binaerformat

| Offset | Groesse | Inhalt |
|--------|---------|--------|
| 0 | 4 | Magic `HB01` |
| 4 | 1 | Version `1` |
| 5 | 1 | Flags `0` |
| 6 | 2 | Title-Laenge (uint16 little-endian) |
| 8 | N | Titel (verschluesselt) |
| 8+N | 4 | Payload-Laenge (uint32 little-endian) |
| 12+N | M | Payload (verschluesselt) |

## Verschluesselung

Stream-XOR mit Schluessel `DarkFoxHB-2026` und positionsabhaengigem Mixer.
Titel startet bei Offset 0 im Keystream, Payload bei Offset = Title-Laenge.

Nicht fuer echte Geheimnisse gedacht – schuetzt vor einfachem Mitlesen auf der SD.

## Dateien erzeugen (PC)

```bash
python tools/pack_homebrew.py "Mein Homebrew" --text "Hallo Welt" mein.hb
python tools/pack_homebrew.py "Levelpack" data.bin level.hb
```

`.hb` auf die SD kopieren, auf dem Display **HomeBrew** oeffnen.

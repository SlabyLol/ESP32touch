# DarkFox HomeBrew Format (.hb)

## Builder

```bash
python tools/pack_homebrew.py --build
```

Quellen: `homebrew/src/<projekt>/`
Ausgabe: `homebrew/out/*.hb`

Siehe auch `homebrew/README.md`.

## Dateiendung

`.hb`

## SD-Ordner

- `/`
- `/homebrew/`
- `/HomeBrew/`
- `/hb/`

## Binaerformat

| Offset | Groesse | Inhalt |
|--------|---------|--------|
| 0 | 4 | Magic `HB01` |
| 4 | 1 | Version `1` |
| 5 | 1 | Flags |
| 6 | 2 | Title-Laenge (uint16 LE) |
| 8 | N | Titel verschluesselt |
| 8+N | 4 | Payload-Laenge (uint32 LE) |
| 12+N | M | Payload verschluesselt |

Payload enthaelt vorne Meta-Zeilen aus info.json:

```
# HomeBrew
title=...
author=...
version=...
description=...
---
<payload datei>
```

## Verschluesselung

Stream-XOR, Schluessel `DarkFoxHB-2026`.

# DarkFox HomeBrew

## Programmieren

Scripts in `homebrew/src/<name>/payload.hbs`, bauen mit:

```bash
python tools/pack_homebrew.py --build
```

Befehle: CLEAR, COLOR, TEXT, CENTER, WAIT, WAIT_TOUCH, BTN

## Format .hb

Magic `HB01`, Titel + Payload stream-XOR mit `DarkFoxHB-2026`.

SD: `/`, `/homebrew/`, `/hb/`

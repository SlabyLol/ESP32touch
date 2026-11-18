# HomeBrew – echte GUI

```python
clear()
bg("BLACK")
color("CYAN")
box(8, 8, 304, 50)
center(30, "Titel")
label(20, 70, "Text")
button(1, 20, 180, 120, 40, "OK")
button(2, 180, 180, 120, 40, "Back")
show()  # wartet auf Touch auf einen Button
```

## GUI-API

| Aufruf | Bedeutung |
|--------|-----------|
| `clear()` | Screen + GUI reset |
| `bg("BLACK")` | Hintergrund |
| `color("CYAN")` | Vordergrundfarbe |
| `box(x,y,w,h)` | Rahmen |
| `fillbox(x,y,w,h)` | Gefülltes Rechteck |
| `label(x,y,"text")` | Text |
| `center(y,"text")` | Zentrierter Text |
| `button(id,x,y,w,h,"text")` | Touch-Button |
| `show()` | Event-Loop bis Button-Touch |
| `wifi()` | WLAN-Status |
| `http("https://...")` | HTTPS GET |
| `wait(ms)` / `wait_touch()` | Warten |

```bash
python tools/pack_homebrew.py --build
```

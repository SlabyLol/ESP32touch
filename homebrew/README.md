# HomeBrew

Quellen sind **echtes** Python / C++ / C# mit API-Aufrufen.
Der Packer übersetzt sie nach `.hb` für den ESP32.

```python
def main():
    clear()
    color("CYAN")
    center(25, "Hallo")
    wifi()
    http("https://api.ipify.org")
    wait_touch()
```

```cpp
void main() {
  clear();
  color("ORANGE");
  http("https://api.ipify.org");
  wait_touch();
}
```

```csharp
HomeBrew.clear();
HomeBrew.http("https://httpbin.org/get");
HomeBrew.wait_touch();
```

## API

| Aufruf | Wirkung |
|--------|--------|
| `clear()` | Screen löschen |
| `color("CYAN")` | Farbe |
| `text(x, y, "msg")` | Text |
| `center(y, "msg")` | Zentriert |
| `wait(ms)` | Pause |
| `wait_touch()` | Warten auf Touch |
| `wifi()` | WLAN-Status |
| `http("https://...")` | HTTPS GET |

## Bauen

```bash
python tools/pack_homebrew.py --build
```

Nur `.py` / `.cpp` / `.cs` → `homebrew/out/*.hb`

## Beispiele

| Datei | .hb |
|-------|-----|
| `net_wifi/main.py` | WLAN Status |
| `net_ipapi/main.cpp` | Public IP |
| `net_httpbin/main.cs` | HTTP Echo |

# HomeBrew

1. Quelle nur **`.py` / `.cpp` / `.cs`**
2. `python tools/pack_homebrew.py --build` → **`.hb`**
3. `.hb` auf SD nach `/homebrew/`

## Internet-Befehle

| Befehl | Wirkung |
|--------|--------|
| `WIFI` | SSID, lokale IP, RSSI |
| `HTTP https://...` | HTTPS GET, Status + Antworttext |

Braucht WLAN (`info.txt` auf der SD).

## Drei Beispiele

| .hb | Quelle | Was es macht |
|-----|--------|----------------|
| `net_wifi.hb` | `main.py` | WLAN-Status live |
| `net_ipapi.hb` | `main.cpp` | Public IP von api.ipify.org |
| `net_httpbin.hb` | `main.cs` | GET httpbin.org/get |

## Weitere Befehle

`CLEAR` `COLOR` `TEXT x y` `CENTER y` `WAIT ms` `WAIT_TOUCH`

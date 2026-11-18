# DarkFox ESP32touch – Arduino IDE Tutorial

Komplette Anleitung: Sketch öffnen, Bibliotheken, TFT_eSPI, SD, flashen, HomeBrew.

## 1. Was du brauchst

- **ESP32** Board (z.B. ESP32-WROOM DevKit)
- Display **ST7789 240x320** + **XPT2046 Touch** (wie im Projekt verdrahtet)
- **Micro-SD** Modul am VSPI (CS=5, MOSI=23, MISO=19, SCK=18)
- USB-Kabel
- PC mit **Arduino IDE 2.x** (oder 1.8.19)

### Pinbelegung (wie im Code)

| Funktion | GPIO |
|----------|------|
| TFT MOSI | 13 |
| TFT SCLK | 14 |
| TFT CS   | 15 |
| TFT DC   | 2 |
| TFT RST  | 35 |
| TFT BL   | 27 |
| TFT MISO | 12 |
| Touch CS | 33 |
| SD CS    | 5 |
| SD MOSI  | 23 |
| SD MISO  | 19 |
| SD SCK   | 18 |

## 2. Arduino IDE – ESP32 Board installieren

1. Arduino IDE öffnen → **File → Preferences**
2. Bei **Additional Board Manager URLs** eintragen:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

3. **Tools → Board → Boards Manager** → nach **esp32** suchen → **esp32 by Espressif Systems** installieren
4. Board wählen: **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
5. Empfohlene Einstellungen:
   - Upload Speed: **921600**
   - Flash Frequency: **80MHz**
   - Flash Mode: **QIO**
   - Partition Scheme: **Default**
   - Port: deinen COM-/ttyUSB-Port

## 3. Bibliothek TFT_eSPI

1. **Tools → Manage Libraries** → suche **TFT_eSPI** (Author: Bodmer) → Install
2. TFT_eSPI muss an euer Display angepasst werden:

Pfad (Beispiel Windows):

```
Dokumente/Arduino/libraries/TFT_eSPI/User_Setup.h
```

macOS/Linux:

```
~/Arduino/libraries/TFT_eSPI/User_Setup.h
```

3. Inhalt von `arduino/TFT_eSPI_User_Setup_Hinweis.h` aus diesem Repo in `User_Setup.h` übernehmen  
   **oder** die Defines unten manuell setzen und andere Driver auskommentieren.

Wichtige Defines:

```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  35
#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH
#define TOUCH_CS 33
#define LOAD_GFXFF
#define SMOOTH_FONT
#define SPI_FREQUENCY 40000000
#define USE_HSPI_PORT
```

4. Arduino IDE **neu starten** nach Änderung an User_Setup.h

## 4. Sketch öffnen

1. Repo klonen oder ZIP von GitHub laden
2. Ordner öffnen:

```
arduino/ESP32touch/ESP32touch.ino
```

3. Im selben Ordner muss liegen:

```
ESP32touch.ino
homebrew_runtime.inc
```

Arduino IDE öffnet den Ordner als Sketch. Nicht nur die .ino isoliert verschieben.

## 5. Kompilieren und hochladen

1. Board + Port prüfen
2. **Sketch → Verify/Compile** (Haken)
3. Bei Erfolg: **Upload** (Pfeil)
4. Serial Monitor: **115200 Baud**

Nach dem Start: **PIN eingeben** (Standard: `1234`) per Touch-Tastenfeld oder Serial.

## 6. SD-Karte vorbereiten

FAT32 formatieren. Dateien im Root bzw. Unterordner:

```
/info.txt          <- WLAN
/list.txt          <- optionale Lauftexte
/wads/             <- optionale .wad Dateien
/homebrew/         <- .hb Dateien
```

### info.txt

```
SSID:DeinWLAN
PASS:DeinPasswort
```

### list.txt (optional, eine Zeile pro Nachricht)

```
Willkommen
DarkFox Station
```

### HomeBrew (.hb)

Auf dem PC im Repo:

```bash
python tools/pack_homebrew.py --build
```

Dann `homebrew/out/*.hb` nach `/homebrew/` auf die SD kopieren.

Oder fertige `.hb` aus dem GitHub Release laden.

## 7. Bedienung auf dem Gerät

1. PIN `1234`
2. Hauptbildschirm: Uhr (wenn WLAN/NTP ok), Lauftexte
3. Button **WADs** → Liste der .wad auf der SD
4. Button **HomeBrew** → Liste der .hb → **OPEN** startet das Programm (GUI, WLAN, HTTP)

## 8. HomeBrew selbst schreiben

Nur `.py` / `.cpp` / `.cs` mit API, dann packen:

```python
def main():
    clear()
    bg("BLACK")
    color("CYAN")
    box(8, 8, 304, 50)
    center(30, "Hallo")
    button(1, 100, 180, 120, 40, "OK")
    show()
```

```bash
python tools/pack_homebrew.py --build
```

API: `clear`, `bg`, `color`, `box`, `label`, `center`, `button`, `show`, `wifi`, `http`, `wait`, `wait_touch`

## 9. Typische Fehler

| Problem | Lösung |
|---------|--------|
| Display schwarz | TFT_eSPI User_Setup Pins/Driver prüfen, Backlight GPIO 27 |
| Touch falsch | `calData` in der .ino anpassen (TFT_eSPI Touch_calibrate Beispiel) |
| SD-Fehler | Verkabelung VSPI, CS=5, FAT32 |
| WLAN offline | info.txt Format, 2.4 GHz WLAN |
| Compile error TFT_eSPI | Library installiert? User_Setup.h gespeichert? IDE neu gestartet? |
| homebrew_runtime.inc not found | .ino und .inc müssen im **gleichen** Ordner `ESP32touch/` liegen |
| Keine .hb | `pack_homebrew.py --build` ausführen, Dateien nach `/homebrew/` |

## 10. PIN ändern

In `ESP32touch.ino`:

```cpp
const char* CONSOLE_PIN = "1234";
```

## 11. Unterschied PlatformIO vs Arduino IDE

| | PlatformIO | Arduino IDE |
|--|------------|-------------|
| Config | `platformio.ini` build_flags | `TFT_eSPI/User_Setup.h` |
| Sketch | `src/main.cpp` | `arduino/ESP32touch/ESP32touch.ino` |
| Code | derselbe Funktionsumfang | derselbe Funktionsumfang |

## 12. Release / fertige Binaries

GitHub → **Releases** → `ESP32touch-*.bin`  
Einfacher: über die Arduino IDE flashen.

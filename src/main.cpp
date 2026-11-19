#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

// CYD / gelbes Board: Backlight = GPIO 21
#ifndef TFT_BL_PIN
#define TFT_BL_PIN 21
#endif

#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18
// WICHTIG: SD auf HSPI. TFT_eSPI belegt standardmaessig VSPI (SPI);
// eine zweite Instanz auf VSPI wuerde dem Display die Pins wegnehmen.
SPIClass sdSPI(HSPI);

const char* CONSOLE_PIN = "1234";

String wifiSSID;
String wifiPASS;

const char* defaultTexts[] = {
  "Willkommen!",
  "DarkFox Station",
  "Naechster Zug: puenktlich",
  "Habt einen schoenen Tag",
  "Powered by ESP32"
};
const int defaultTextCount = sizeof(defaultTexts) / sizeof(defaultTexts[0]);

#define MAX_TEXTS 30
String rotatingTexts[MAX_TEXTS];
int rotatingTextCount = 0;
int currentTextIndex = 0;
unsigned long lastTextSwitch = 0;
const unsigned long TEXT_SWITCH_MS = 4000;

unsigned long lastClockUpdate = 0;
const unsigned long CLOCK_UPDATE_MS = 1000;
bool timeReady = false;

#define HB_MAGIC0 'H'
#define HB_MAGIC1 'B'
#define HB_MAGIC2 '0'
#define HB_MAGIC3 '1'
#define HB_VERSION 1
#define HB_KEY "DarkFoxHB-2026"

#define MAX_WADS 40
#define MAX_HB 40
String wadList[MAX_WADS];
int wadCount = 0;
int wadScroll = 0;
int wadSelected = 0;

struct HbEntry {
  String path;
  String title;
  uint32_t payloadSize;
};
HbEntry hbList[MAX_HB];
int hbCount = 0;
int hbScroll = 0;
int hbSelected = 0;

enum MenuMode { MODE_HOME, MODE_WAD, MODE_HB };
MenuMode menuMode = MODE_HOME;

const int BTN1_X = 10,  BTN1_Y = 210, BTN1_W = 90,  BTN1_H = 35;
const int BTN2_X = 115, BTN2_Y = 210, BTN2_W = 100, BTN2_H = 35;

const int LIST_TOP = 40;
const int ROW_H = 28;
const int VISIBLE = 5;

const int BTN_BACK_X = 5,   BTN_BACK_Y = 200, BTN_BACK_W = 70,  BTN_BACK_H = 32;
const int BTN_UP_X   = 85,  BTN_UP_Y   = 200, BTN_UP_W   = 50,  BTN_UP_H   = 32;
const int BTN_DOWN_X = 145, BTN_DOWN_Y = 200, BTN_DOWN_W = 50,  BTN_DOWN_H = 32;
const int BTN_RUN_X  = 205, BTN_RUN_Y  = 200, BTN_RUN_W  = 105, BTN_RUN_H  = 32;

unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 280;

// Touch-Kalibrierung ist rotationsabhaengig: PIN-Screen = Hochformat (0), Rest = Querformat (1).
// Eigene Werte: Env s032_calibrate flashen und Serial-Ausgabe hier eintragen.
uint16_t calPortrait[5]  = { 300, 3600, 300, 3600, 0 };
uint16_t calLandscape[5] = { 300, 3600, 300, 3600, 1 };

struct KeyButton { int x, y, w, h; char label[4]; };
KeyButton keys[12];
const int KEY_W = 70, KEY_H = 45, KEY_GAP = 6, KEYPAD_TOP = 115;

void buildKeypad() {
  const char* labels[12] = {"1","2","3","4","5","6","7","8","9","C","0","OK"};
  int startX = (tft.width() - (3 * KEY_W + 2 * KEY_GAP)) / 2;
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      int idx = row * 3 + col;
      keys[idx].x = startX + col * (KEY_W + KEY_GAP);
      keys[idx].y = KEYPAD_TOP + row * (KEY_H + KEY_GAP);
      keys[idx].w = KEY_W;
      keys[idx].h = KEY_H;
      strcpy(keys[idx].label, labels[idx]);
    }
  }
}

void drawKeypad() {
  for (int i = 0; i < 12; i++) {
    tft.drawRect(keys[i].x, keys[i].y, keys[i].w, keys[i].h, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(keys[i].label, keys[i].x + keys[i].w / 2, keys[i].y + keys[i].h / 2);
  }
}

void drawPinDisplay(const String &input) {
  tft.fillRect(0, 55, tft.width(), 50, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String masked = "";
  for (unsigned int i = 0; i < input.length(); i++) masked += "*";
  tft.drawString(masked, tft.width() / 2, 80);
}

void drawPasswordScreen() {
  tft.setRotation(0);
  tft.setTouch(calPortrait);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("PIN eingeben", tft.width() / 2, 25);
  buildKeypad();
  drawKeypad();
  drawPinDisplay("");
}

char getPressedKey(int tx, int ty) {
  for (int i = 0; i < 12; i++) {
    if (tx >= keys[i].x && tx <= keys[i].x + keys[i].w &&
        ty >= keys[i].y && ty <= keys[i].y + keys[i].h) {
      return keys[i].label[0] == 'O' ? 'O' : (keys[i].label[0] == 'C' ? 'C' : keys[i].label[0]);
    }
  }
  return 0;
}

bool waitForPin() {
  drawPasswordScreen();
  String input = "";
  unsigned long lastTouch = 0;
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        if (input == CONSOLE_PIN) { tft.setRotation(1); tft.setTouch(calLandscape); return true; }
        input = ""; drawPinDisplay(input);
      } else if (c == 8 || c == 127) {
        if (input.length() > 0) input.remove(input.length() - 1);
        drawPinDisplay(input);
      } else if (isDigit(c)) {
        input += c; drawPinDisplay(input);
      }
    }
    uint16_t tx, ty;
    if (millis() - lastTouch > 250 && tft.getTouch(&tx, &ty)) {
      lastTouch = millis();
      char key = getPressedKey(tx, ty);
      if (key == 'C') { input = ""; drawPinDisplay(input); }
      else if (key == 'O') {
        if (input == CONSOLE_PIN) { tft.setRotation(1); tft.setTouch(calLandscape); return true; }
        input = ""; drawPinDisplay(input);
      } else if (key >= '0' && key <= '9') {
        input += key; drawPinDisplay(input);
      }
    }
  }
}

uint8_t hbKeyByte(uint32_t pos) {
  const char* k = HB_KEY;
  size_t len = strlen(k);
  uint8_t b = (uint8_t)k[pos % len];
  b ^= (uint8_t)(pos * 31u + 17u);
  b ^= (uint8_t)(pos >> 8);
  return b;
}

void hbCrypt(uint8_t* data, size_t len, uint32_t offset) {
  for (size_t i = 0; i < len; i++) {
    data[i] ^= hbKeyByte(offset + (uint32_t)i);
  }
}

bool loadWifiCredentials() {
  File f = SD.open("/info.txt");
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("SSID:")) { wifiSSID = line.substring(5); wifiSSID.trim(); }
    else if (line.startsWith("PASS:")) { wifiPASS = line.substring(5); wifiPASS.trim(); }
  }
  f.close();
  return wifiSSID.length() > 0;
}

void loadRotatingTexts() {
  File f = SD.open("/list.txt");
  if (!f) {
    for (int i = 0; i < defaultTextCount; i++) rotatingTexts[i] = String(defaultTexts[i]);
    rotatingTextCount = defaultTextCount;
    return;
  }
  rotatingTextCount = 0;
  while (f.available() && rotatingTextCount < MAX_TEXTS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) rotatingTexts[rotatingTextCount++] = line;
  }
  f.close();
  if (rotatingTextCount == 0) {
    for (int i = 0; i < defaultTextCount; i++) rotatingTexts[i] = String(defaultTexts[i]);
    rotatingTextCount = defaultTextCount;
  }
}

bool endsWithIgnoreCase(const String& name, const char* ext) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(ext);
}

void scanWadsInDir(const char* path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry && wadCount < MAX_WADS) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      if (endsWithIgnoreCase(name, ".wad")) {
        String fullPath = String(path);
        if (!fullPath.endsWith("/")) fullPath += "/";
        if (fullPath == "//") fullPath = "/";
        fullPath += name;
        wadList[wadCount++] = fullPath;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void scanAllWads() {
  wadCount = 0; wadScroll = 0; wadSelected = 0;
  scanWadsInDir("/");
  scanWadsInDir("/wads");
  scanWadsInDir("/WADS");
  scanWadsInDir("/doom");
  scanWadsInDir("/DOOM");
}

bool readHomeBrewHeader(const String& path, String& outTitle, uint32_t& outPayloadSize) {
  File f = SD.open(path);
  if (!f) return false;
  if (f.size() < 12) { f.close(); return false; }
  uint8_t hdr[8];
  if (f.read(hdr, 8) != 8) { f.close(); return false; }
  if (hdr[0] != HB_MAGIC0 || hdr[1] != HB_MAGIC1 || hdr[2] != HB_MAGIC2 || hdr[3] != HB_MAGIC3) {
    f.close(); return false;
  }
  if (hdr[4] != HB_VERSION) { f.close(); return false; }
  uint16_t titleLen = (uint16_t)hdr[6] | ((uint16_t)hdr[7] << 8);
  if (titleLen == 0 || titleLen > 64) { f.close(); return false; }
  uint8_t titleBuf[65];
  if (f.read(titleBuf, titleLen) != (int)titleLen) { f.close(); return false; }
  hbCrypt(titleBuf, titleLen, 0);
  titleBuf[titleLen] = 0;
  outTitle = String((char*)titleBuf);
  uint8_t sz[4];
  if (f.read(sz, 4) != 4) { f.close(); return false; }
  outPayloadSize = (uint32_t)sz[0] | ((uint32_t)sz[1] << 8) | ((uint32_t)sz[2] << 16) | ((uint32_t)sz[3] << 24);
  f.close();
  return true;
}

void scanHbInDir(const char* path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry && hbCount < MAX_HB) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      if (endsWithIgnoreCase(name, ".hb")) {
        String fullPath = String(path);
        if (!fullPath.endsWith("/")) fullPath += "/";
        if (fullPath == "//") fullPath = "/";
        fullPath += name;
        String title;
        uint32_t payloadSize = 0;
        if (readHomeBrewHeader(fullPath, title, payloadSize)) {
          hbList[hbCount].path = fullPath;
          hbList[hbCount].title = title;
          hbList[hbCount].payloadSize = payloadSize;
          hbCount++;
        }
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void scanAllHomeBrew() {
  hbCount = 0; hbScroll = 0; hbSelected = 0;
  scanHbInDir("/");
  scanHbInDir("/homebrew");
  scanHbInDir("/HomeBrew");
  scanHbInDir("/hb");
}

bool hit(int tx, int ty, int x, int y, int w, int h) {
  return tx >= x && tx <= x + w && ty >= y && ty <= y + h;
}

void showStatus(const char* text) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
  tft.drawString(text, tft.width() / 2, 205);
}

void drawMainButton(int x, int y, int w, int h, const char* label, uint16_t border) {
  tft.fillRect(x, y, w, h, TFT_DARKGREY);
  tft.drawRect(x, y, w, h, border);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(label, x + w / 2, y + h / 2);
}

void drawStaticLayout() {
  menuMode = MODE_HOME;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.drawString("DarkFox", tft.width() / 2, 40);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.drawString("Raspberry Pi Station", tft.width() / 2, 80);
  drawMainButton(BTN1_X, BTN1_Y, BTN1_W, BTN1_H, "WADs", TFT_RED);
  drawMainButton(BTN2_X, BTN2_Y, BTN2_W, BTN2_H, "HomeBrew", TFT_MAGENTA);
}

void updateClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return;
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.fillRect(tft.width() / 2 - 70, 100, 140, 30, TFT_BLACK);
  tft.drawString(buf, tft.width() / 2, 115);
}

void updateRotatingText() {
  if (rotatingTextCount == 0 || menuMode != MODE_HOME) return;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);
  tft.drawString(rotatingTexts[currentTextIndex], tft.width() / 2, 180);
}

void drawListButton(int x, int y, int w, int h, const char* label, uint16_t border) {
  tft.fillRect(x, y, w, h, TFT_DARKGREY);
  tft.drawRect(x, y, w, h, border);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(label, x + w / 2, y + h / 2);
}

void drawWadMenu() {
  menuMode = MODE_WAD;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("WAD Runner", 10, 8);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  char countBuf[24];
  snprintf(countBuf, sizeof(countBuf), "%d gefunden", wadCount);
  tft.drawString(countBuf, 200, 12);
  if (wadCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine WADs auf SD", tft.width() / 2, 100);
  } else {
    for (int i = 0; i < VISIBLE; i++) {
      int idx = wadScroll + i;
      if (idx >= wadCount) break;
      int y = LIST_TOP + i * ROW_H;
      bool sel = (idx == wadSelected);
      tft.fillRect(5, y, tft.width() - 10, ROW_H - 2, sel ? TFT_NAVY : TFT_BLACK);
      tft.drawRect(5, y, tft.width() - 10, ROW_H - 2, sel ? TFT_YELLOW : TFT_DARKGREY);
      String name = wadList[idx];
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      tft.setTextDatum(ML_DATUM);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, sel ? TFT_NAVY : TFT_BLACK);
      tft.drawString(name, 12, y + (ROW_H - 2) / 2);
    }
  }
  drawListButton(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, "Back", TFT_WHITE);
  drawListButton(BTN_UP_X, BTN_UP_Y, BTN_UP_W, BTN_UP_H, "Up", TFT_CYAN);
  drawListButton(BTN_DOWN_X, BTN_DOWN_Y, BTN_DOWN_W, BTN_DOWN_H, "Dn", TFT_CYAN);
  drawListButton(BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H, "START", TFT_GREEN);
}

void drawHbMenu() {
  menuMode = MODE_HB;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.drawString("HomeBrew", 10, 8);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  char countBuf[24];
  snprintf(countBuf, sizeof(countBuf), "%d gefunden", hbCount);
  tft.drawString(countBuf, 200, 12);
  if (hbCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine .hb Dateien", tft.width() / 2, 90);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("tools/pack_homebrew.py", tft.width() / 2, 120);
    tft.drawString("auf PC nutzen", tft.width() / 2, 145);
  } else {
    for (int i = 0; i < VISIBLE; i++) {
      int idx = hbScroll + i;
      if (idx >= hbCount) break;
      int y = LIST_TOP + i * ROW_H;
      bool sel = (idx == hbSelected);
      tft.fillRect(5, y, tft.width() - 10, ROW_H - 2, sel ? TFT_PURPLE : TFT_BLACK);
      tft.drawRect(5, y, tft.width() - 10, ROW_H - 2, sel ? TFT_YELLOW : TFT_DARKGREY);
      tft.setTextDatum(ML_DATUM);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, sel ? TFT_PURPLE : TFT_BLACK);
      tft.drawString(hbList[idx].title, 12, y + (ROW_H - 2) / 2);
    }
  }
  drawListButton(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, "Back", TFT_WHITE);
  drawListButton(BTN_UP_X, BTN_UP_Y, BTN_UP_W, BTN_UP_H, "Up", TFT_CYAN);
  drawListButton(BTN_DOWN_X, BTN_DOWN_Y, BTN_DOWN_W, BTN_DOWN_H, "Dn", TFT_CYAN);
  drawListButton(BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H, "OPEN", TFT_GREEN);
}

void showWadRunScreen(int idx) {
  String path = wadList[idx];
  String name = path;
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  uint32_t size = 0;
  File f = SD.open(path);
  if (f) { size = f.size(); f.close(); }
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("WAD ausgewaehlt", tft.width() / 2, 50);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(name, tft.width() / 2, 90);
  char sizeBuf[32];
  snprintf(sizeBuf, sizeof(sizeBuf), size >= 1024*1024 ? "%.1f MB" : "%u KB",
           size >= 1024*1024 ? size/(1024.0f*1024.0f) : (unsigned)(size/1024));
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(sizeBuf, tft.width() / 2, 120);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Datei auf SD bereit", tft.width() / 2, 160);
  delay(2500);
  drawWadMenu();
}

#include "homebrew_runtime.inc"

void handleListNav(int tx, int ty, int &selected, int &scroll, int count, void (*redraw)(), void (*runFn)(int)) {
  if (hit(tx, ty, BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H)) {
    drawStaticLayout();
    updateRotatingText();
    if (timeReady) updateClock();
    return;
  }
  if (count == 0) return;
  if (hit(tx, ty, BTN_UP_X, BTN_UP_Y, BTN_UP_W, BTN_UP_H)) {
    if (selected > 0) {
      selected--;
      if (selected < scroll) scroll = selected;
      redraw();
    }
    return;
  }
  if (hit(tx, ty, BTN_DOWN_X, BTN_DOWN_Y, BTN_DOWN_W, BTN_DOWN_H)) {
    if (selected < count - 1) {
      selected++;
      if (selected >= scroll + VISIBLE) scroll = selected - VISIBLE + 1;
      redraw();
    }
    return;
  }
  if (hit(tx, ty, BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H)) {
    runFn(selected);
    return;
  }
  for (int i = 0; i < VISIBLE; i++) {
    int idx = scroll + i;
    if (idx >= count) break;
    int y = LIST_TOP + i * ROW_H;
    if (hit(tx, ty, 5, y, tft.width() - 10, ROW_H - 2)) {
      selected = idx;
      redraw();
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Backlight ZUERST an (CYD = GPIO 21)
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.drawString("DarkFox boot...", tft.width() / 2, tft.height() / 2);

  Serial.println("=== DarkFox Konsole ===");
  Serial.printf("Backlight pin: %d\n", TFT_BL_PIN);

  waitForPin();
  drawStaticLayout();
  showStatus("Lese SD-Karte...");
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool sdOk = SD.begin(SD_CS, sdSPI);
  if (!sdOk) {
    showStatus("SD-Fehler");
    for (int i = 0; i < defaultTextCount; i++) rotatingTexts[i] = String(defaultTexts[i]);
    rotatingTextCount = defaultTextCount;
  } else {
    loadRotatingTexts();
    scanAllWads();
    scanAllHomeBrew();
    loadWifiCredentials();
  }
  showStatus("Verbinde WLAN...");
  WiFi.mode(WIFI_STA);
  if (wifiSSID.length() > 0) {
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    showStatus("Hole Uhrzeit...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) timeReady = true;
  } else {
    showStatus("WLAN offline");
  }
  tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
  updateRotatingText();
  drawMainButton(BTN1_X, BTN1_Y, BTN1_W, BTN1_H, "WADs", TFT_RED);
  drawMainButton(BTN2_X, BTN2_Y, BTN2_W, BTN2_H, "HomeBrew", TFT_MAGENTA);
}

void loop() {
  if (menuMode == MODE_HOME) {
    if (timeReady && millis() - lastClockUpdate > CLOCK_UPDATE_MS) {
      lastClockUpdate = millis();
      updateClock();
    }
    if (rotatingTextCount > 0 && millis() - lastTextSwitch > TEXT_SWITCH_MS) {
      lastTextSwitch = millis();
      currentTextIndex = (currentTextIndex + 1) % rotatingTextCount;
      updateRotatingText();
    }
  }
  uint16_t tx, ty;
  if (millis() - lastTouchTime > TOUCH_DEBOUNCE_MS && tft.getTouch(&tx, &ty)) {
    lastTouchTime = millis();
    if (menuMode == MODE_WAD) {
      handleListNav(tx, ty, wadSelected, wadScroll, wadCount, drawWadMenu, showWadRunScreen);
    } else if (menuMode == MODE_HB) {
      handleListNav(tx, ty, hbSelected, hbScroll, hbCount, drawHbMenu, showHbOpenScreen);
    } else {
      if (hit(tx, ty, BTN1_X, BTN1_Y, BTN1_W, BTN1_H)) {
        scanAllWads();
        drawWadMenu();
      } else if (hit(tx, ty, BTN2_X, BTN2_Y, BTN2_W, BTN2_H)) {
        scanAllHomeBrew();
        drawHbMenu();
      }
    }
  }
}

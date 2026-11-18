#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

// ---------- Display ----------
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL_PIN 27

// ---------- SD-Karte (Standard-VSPI-Bus dieses Boards) ----------
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18
SPIClass sdSPI(VSPI);

// ---------- PIN ----------
const char* CONSOLE_PIN = "1234";

// ---------- WLAN ----------
String wifiSSID;
String wifiPASS;

// ---------- Rotierende Texte ----------
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

// ---------- WAD Runner ----------
#define MAX_WADS 50
String wadList[MAX_WADS];
int wadCount = 0;
int wadScroll = 0;
int wadSelected = 0;
bool inWadMenu = false;

const int DOOM_BTN_X = 10;
const int DOOM_BTN_Y = 210;
const int DOOM_BTN_W = 100;
const int DOOM_BTN_H = 35;

const int WAD_LIST_TOP = 40;
const int WAD_ROW_H = 28;
const int WAD_VISIBLE = 5;

const int BTN_BACK_X = 5;
const int BTN_BACK_Y = 200;
const int BTN_BACK_W = 70;
const int BTN_BACK_H = 32;

const int BTN_UP_X = 85;
const int BTN_UP_Y = 200;
const int BTN_UP_W = 50;
const int BTN_UP_H = 32;

const int BTN_DOWN_X = 145;
const int BTN_DOWN_Y = 200;
const int BTN_DOWN_W = 50;
const int BTN_DOWN_H = 32;

const int BTN_RUN_X = 205;
const int BTN_RUN_Y = 200;
const int BTN_RUN_W = 105;
const int BTN_RUN_H = 32;

unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 280;

// ---------- Touch-Kalibrierung ----------
uint16_t calData[5] = { 300, 3600, 300, 3600, 1 };

// ---------- PIN-Keypad ----------
struct KeyButton {
  int x, y, w, h;
  char label[4];
};
KeyButton keys[12];
const int KEY_W = 70;
const int KEY_H = 45;
const int KEY_GAP = 6;
const int KEYPAD_TOP = 115;

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
        if (input == CONSOLE_PIN) {
          tft.setRotation(1);
          return true;
        }
        Serial.println("Falsche PIN");
        input = "";
        drawPinDisplay(input);
      } else if (c == '\r') {
      } else if (c == 8 || c == 127) {
        if (input.length() > 0) input.remove(input.length() - 1);
        drawPinDisplay(input);
      } else if (isDigit(c)) {
        input += c;
        drawPinDisplay(input);
      }
    }

    uint16_t tx, ty;
    if (millis() - lastTouch > 250 && tft.getTouch(&tx, &ty)) {
      lastTouch = millis();
      char key = getPressedKey(tx, ty);
      if (key == 'C') {
        input = "";
        drawPinDisplay(input);
      } else if (key == 'O') {
        if (input == CONSOLE_PIN) {
          tft.setRotation(1);
          return true;
        }
        input = "";
        drawPinDisplay(input);
      } else if (key >= '0' && key <= '9') {
        input += key;
        drawPinDisplay(input);
      }
    }
  }
}

// ---------- SD Helpers ----------
bool loadWifiCredentials() {
  File f = SD.open("/info.txt");
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("SSID:")) {
      wifiSSID = line.substring(5);
      wifiSSID.trim();
    } else if (line.startsWith("PASS:")) {
      wifiPASS = line.substring(5);
      wifiPASS.trim();
    }
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
    if (line.length() > 0) {
      rotatingTexts[rotatingTextCount++] = line;
    }
  }
  f.close();
  if (rotatingTextCount == 0) {
    for (int i = 0; i < defaultTextCount; i++) rotatingTexts[i] = String(defaultTexts[i]);
    rotatingTextCount = defaultTextCount;
  }
}

bool isWadFile(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".wad");
}

void scanWadsInDir(const char* path) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;

  File entry = dir.openNextFile();
  while (entry && wadCount < MAX_WADS) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      // name kann voller Pfad sein
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);

      if (isWadFile(name)) {
        String fullPath = String(path);
        if (!fullPath.endsWith("/")) fullPath += "/";
        if (fullPath == "//") fullPath = "/";
        fullPath += name;
        wadList[wadCount] = fullPath;
        wadCount++;
        Serial.printf("WAD gefunden: %s (%u bytes)\n", fullPath.c_str(), entry.size());
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void scanAllWads() {
  wadCount = 0;
  wadScroll = 0;
  wadSelected = 0;
  scanWadsInDir("/");
  scanWadsInDir("/wads");
  scanWadsInDir("/WADS");
  scanWadsInDir("/doom");
  scanWadsInDir("/DOOM");
  Serial.printf("Gesamt %d WADs gefunden\n", wadCount);
}

// ---------- UI ----------
void showStatus(const char* text) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
  tft.drawString(text, tft.width() / 2, 205);
}

void drawDoomButton() {
  tft.fillRect(DOOM_BTN_X, DOOM_BTN_Y, DOOM_BTN_W, DOOM_BTN_H, TFT_DARKGREY);
  tft.drawRect(DOOM_BTN_X, DOOM_BTN_Y, DOOM_BTN_W, DOOM_BTN_H, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("WADs", DOOM_BTN_X + DOOM_BTN_W / 2, DOOM_BTN_Y + DOOM_BTN_H / 2);
}

void drawStaticLayout() {
  inWadMenu = false;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.drawString("DarkFox", tft.width() / 2, 40);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.drawString("Raspberry Pi Station", tft.width() / 2, 80);

  drawDoomButton();
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
  if (rotatingTextCount == 0 || inWadMenu) return;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);
  tft.drawString(rotatingTexts[currentTextIndex], tft.width() / 2, 180);
}

void drawWadButton(int x, int y, int w, int h, const char* label, uint16_t border) {
  tft.fillRect(x, y, w, h, TFT_DARKGREY);
  tft.drawRect(x, y, w, h, border);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(label, x + w / 2, y + h / 2);
}

void drawWadMenu() {
  inWadMenu = true;
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
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Lege .wad Dateien ins Root", tft.width() / 2, 130);
    tft.drawString("oder in /wads/", tft.width() / 2, 155);
  } else {
    for (int i = 0; i < WAD_VISIBLE; i++) {
      int idx = wadScroll + i;
      if (idx >= wadCount) break;

      int y = WAD_LIST_TOP + i * WAD_ROW_H;
      bool sel = (idx == wadSelected);

      if (sel) {
        tft.fillRect(5, y, tft.width() - 10, WAD_ROW_H - 2, TFT_NAVY);
        tft.drawRect(5, y, tft.width() - 10, WAD_ROW_H - 2, TFT_YELLOW);
      } else {
        tft.fillRect(5, y, tft.width() - 10, WAD_ROW_H - 2, TFT_BLACK);
        tft.drawRect(5, y, tft.width() - 10, WAD_ROW_H - 2, TFT_DARKGREY);
      }

      String name = wadList[idx];
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);

      tft.setTextDatum(ML_DATUM);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, sel ? TFT_NAVY : TFT_BLACK);
      tft.drawString(name, 12, y + (WAD_ROW_H - 2) / 2);
    }
  }

  drawWadButton(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, "Back", TFT_WHITE);
  drawWadButton(BTN_UP_X, BTN_UP_Y, BTN_UP_W, BTN_UP_H, "Up", TFT_CYAN);
  drawWadButton(BTN_DOWN_X, BTN_DOWN_Y, BTN_DOWN_W, BTN_DOWN_H, "Dn", TFT_CYAN);
  drawWadButton(BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H, "START", TFT_GREEN);
}

void showWadRunScreen(int idx) {
  String path = wadList[idx];
  String name = path;
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);

  uint32_t size = 0;
  File f = SD.open(path);
  if (f) {
    size = f.size();
    f.close();
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("WAD ausgewaehlt", tft.width() / 2, 40);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(name, tft.width() / 2, 80);

  char sizeBuf[32];
  if (size >= 1024 * 1024) {
    snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", size / (1024.0f * 1024.0f));
  } else {
    snprintf(sizeBuf, sizeof(sizeBuf), "%u KB", (unsigned)(size / 1024));
  }
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(sizeBuf, tft.width() / 2, 110);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Zum echten Spielen", tft.width() / 2, 150);
  tft.drawString("Doom-Firmware flashen", tft.width() / 2, 175);
  tft.drawString("WAD bleibt auf der SD", tft.width() / 2, 200);

  Serial.printf("START WAD: %s (%u bytes)\n", path.c_str(), size);
  delay(3500);
  drawWadMenu();
}

void openWadMenu() {
  scanAllWads();
  drawWadMenu();
}

bool hit(int tx, int ty, int x, int y, int w, int h) {
  return tx >= x && tx <= x + w && ty >= y && ty <= y + h;
}

void handleWadMenuTouch(int tx, int ty) {
  if (hit(tx, ty, BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H)) {
    drawStaticLayout();
    updateRotatingText();
    if (timeReady) updateClock();
    return;
  }

  if (wadCount == 0) return;

  if (hit(tx, ty, BTN_UP_X, BTN_UP_Y, BTN_UP_W, BTN_UP_H)) {
    if (wadSelected > 0) {
      wadSelected--;
      if (wadSelected < wadScroll) wadScroll = wadSelected;
      drawWadMenu();
    }
    return;
  }

  if (hit(tx, ty, BTN_DOWN_X, BTN_DOWN_Y, BTN_DOWN_W, BTN_DOWN_H)) {
    if (wadSelected < wadCount - 1) {
      wadSelected++;
      if (wadSelected >= wadScroll + WAD_VISIBLE) {
        wadScroll = wadSelected - WAD_VISIBLE + 1;
      }
      drawWadMenu();
    }
    return;
  }

  if (hit(tx, ty, BTN_RUN_X, BTN_RUN_Y, BTN_RUN_W, BTN_RUN_H)) {
    showWadRunScreen(wadSelected);
    return;
  }

  // Direkt auf Listeneintrag tippen
  for (int i = 0; i < WAD_VISIBLE; i++) {
    int idx = wadScroll + i;
    if (idx >= wadCount) break;
    int y = WAD_LIST_TOP + i * WAD_ROW_H;
    if (hit(tx, ty, 5, y, tft.width() - 10, WAD_ROW_H - 2)) {
      wadSelected = idx;
      drawWadMenu();
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);

  Serial.println("=== DarkFox Konsole ===");
  waitForPin();

  drawStaticLayout();
  showStatus("Lese SD-Karte...");

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool sdOk = SD.begin(SD_CS, sdSPI);

  if (!sdOk) {
    Serial.println("SD-Fehler");
    showStatus("SD-Fehler, Standardtexte");
    for (int i = 0; i < defaultTextCount; i++) rotatingTexts[i] = String(defaultTexts[i]);
    rotatingTextCount = defaultTextCount;
  } else {
    loadRotatingTexts();
    scanAllWads();
    loadWifiCredentials();
  }

  showStatus("Verbinde WLAN...");
  WiFi.mode(WIFI_STA);
  if (wifiSSID.length() > 0) {
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
      delay(300);
    }
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
  drawDoomButton();
}

void loop() {
  if (!inWadMenu) {
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

    if (inWadMenu) {
      handleWadMenuTouch(tx, ty);
    } else if (hit(tx, ty, DOOM_BTN_X, DOOM_BTN_Y, DOOM_BTN_W, DOOM_BTN_H)) {
      openWadMenu();
    }
  }
}

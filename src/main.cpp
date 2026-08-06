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

// ---------- PIN, die am Serial Monitor ODER per Touch eingegeben werden muss ----------
const char* CONSOLE_PIN = "1234"; // hier anpassen! Nur Ziffern.

// ---------- WLAN-Zugangsdaten (aus info.txt) ----------
String wifiSSID;
String wifiPASS;

// ---------- Standardtexte, falls list.txt auf der SD-Karte fehlt ----------
const char* defaultTexts[] = {
  "Willkommen!",
  "DarkFox Station",
  "Naechster Zug: puenktlich",
  "Habt einen schoenen Tag",
  "Powered by ESP32"
};
const int defaultTextCount = sizeof(defaultTexts) / sizeof(defaultTexts[0]);

// ---------- Rotierende Texte (aus list.txt oder defaultTexts befuellt) ----------
#define MAX_TEXTS 30
String rotatingTexts[MAX_TEXTS];
int rotatingTextCount = 0;
int currentTextIndex = 0;
unsigned long lastTextSwitch = 0;
const unsigned long TEXT_SWITCH_MS = 4000;

unsigned long lastClockUpdate = 0;
const unsigned long CLOCK_UPDATE_MS = 1000;
bool timeReady = false;

// ======================================================================
//  Touch-Kalibrierung
//  Falls die Tasten auf dem Screen nicht dort reagieren, wo man draufdrueckt:
//  Mit TFT_eSPI kommt ein Kalibrierungs-Beispiel-Sketch ("Touch_calibrate"
//  unter examples/Generic), der eigene Werte fuer dein Display ausgibt.
//  Die Werte hier einfach ersetzen.
// ======================================================================
uint16_t calData[5] = { 300, 3600, 300, 3600, 1 };

// ---------- Numerisches Tastenfeld ----------
struct KeyButton {
  int x, y, w, h;
  char label[4];
};

// 3 Spalten x 4 Reihen: 1-9, C(lear), 0, OK
// Layout fuer Hochformat (240x320) - siehe drawPasswordScreen()
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
  tft.setRotation(0); // Hochformat (240x320) - genug Platz fuer 4 Tastenreihen
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("PIN eingeben", tft.width() / 2, 25);

  buildKeypad();
  drawKeypad();
  drawPinDisplay("");
}

// gibt Buchstabe des gedrueckten Tasters zurueck, oder 0 wenn keiner getroffen wurde
char getPressedKey(int tx, int ty) {
  for (int i = 0; i < 12; i++) {
    if (tx >= keys[i].x && tx <= keys[i].x + keys[i].w &&
        ty >= keys[i].y && ty <= keys[i].y + keys[i].h) {
      return keys[i].label[0] == 'O' ? 'O' : (keys[i].label[0] == 'C' ? 'C' : keys[i].label[0]);
    }
  }
  return 0;
}

// ---------- Passwortabfrage: Touch UND Serial parallel ----------
bool waitForPin() {
  drawPasswordScreen();

  String input = "";
  unsigned long lastTouch = 0;
  const unsigned long TOUCH_DEBOUNCE_MS = 250;

  while (true) {
    // --- Serial pruefen ---
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        if (input == CONSOLE_PIN) {
          tft.setRotation(1); // zurueck ins Querformat fuer die Hauptanzeige
          return true;
        }
        Serial.println("Falsche PIN, nochmal versuchen.");
        input = "";
        drawPinDisplay(input);
      } else if (c == '\r') {
        // ignorieren
      } else if (c == 8 || c == 127) { // Backspace
        if (input.length() > 0) input.remove(input.length() - 1);
        drawPinDisplay(input);
      } else if (isDigit(c)) {
        input += c;
        drawPinDisplay(input);
      }
    }

    // --- Touch pruefen ---
    uint16_t tx, ty;
    if (millis() - lastTouch > TOUCH_DEBOUNCE_MS && tft.getTouch(&tx, &ty)) {
      lastTouch = millis();
      char key = getPressedKey(tx, ty);
      if (key == 'C') {
        input = "";
        drawPinDisplay(input);
      } else if (key == 'O') {
        if (input == CONSOLE_PIN) {
          tft.setRotation(1); // zurueck ins Querformat fuer die Hauptanzeige
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

// ---------- SD: info.txt lesen (WLAN-Zugangsdaten) ----------
bool loadWifiCredentials() {
  File f = SD.open("/info.txt");
  if (!f) {
    Serial.println("info.txt nicht gefunden");
    return false;
  }

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

  Serial.printf("SSID gelesen: '%s'\n", wifiSSID.c_str());
  return wifiSSID.length() > 0;
}

// ---------- SD: list.txt lesen (rotierende Nachrichten) ----------
void loadRotatingTexts() {
  File f = SD.open("/list.txt");
  if (!f) {
    Serial.println("list.txt nicht gefunden, benutze Standardtexte aus dem Code");
    for (int i = 0; i < defaultTextCount; i++) {
      rotatingTexts[i] = String(defaultTexts[i]);
    }
    rotatingTextCount = defaultTextCount;
    return;
  }

  rotatingTextCount = 0;
  while (f.available() && rotatingTextCount < MAX_TEXTS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      rotatingTexts[rotatingTextCount] = line;
      rotatingTextCount++;
    }
  }
  f.close();

  if (rotatingTextCount == 0) {
    Serial.println("list.txt war leer, benutze Standardtexte aus dem Code");
    for (int i = 0; i < defaultTextCount; i++) {
      rotatingTexts[i] = String(defaultTexts[i]);
    }
    rotatingTextCount = defaultTextCount;
  } else {
    Serial.printf("%d Nachrichten aus list.txt geladen\n", rotatingTextCount);
  }
}

// ---------- Bildschirm-Layout (Hauptanzeige) ----------
void showStatus(const char* text) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
  tft.drawString(text, tft.width() / 2, 205);
}

void drawStaticLayout() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.drawString("DarkFox", tft.width() / 2, 40);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.drawString("Raspberry Pi Station", tft.width() / 2, 80);
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
  if (rotatingTextCount == 0) return;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);
  tft.drawString(rotatingTexts[currentTextIndex], tft.width() / 2, 180);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Zeit fuer den Serial Monitor zum Verbinden

  // Backlight
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  // Display
  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);

  // PIN-Abfrage - blockiert, bis richtige PIN eingegeben wurde (Touch oder Serial)
  Serial.println("=== DarkFox Konsole ===");
  Serial.println("PIN am Screen eintippen ODER hier im Serial Monitor + Enter.");
  waitForPin();

  drawStaticLayout();
  showStatus("Lese SD-Karte...");

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool sdOk = SD.begin(SD_CS, sdSPI);

  if (!sdOk) {
    Serial.println("SD-Karte konnte nicht initialisiert werden");
    showStatus("SD-Fehler, Standardtexte");
    for (int i = 0; i < defaultTextCount; i++) {
      rotatingTexts[i] = String(defaultTexts[i]);
    }
    rotatingTextCount = defaultTextCount;
  } else {
    loadRotatingTexts();

    if (!loadWifiCredentials()) {
      showStatus("Fehler: info.txt fehlt!");
      updateRotatingText();
      return; // ohne WLAN keine Uhrzeit, aber Texte laufen trotzdem
    }
  }

  showStatus("Verbinde WLAN...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
  }

  if (WiFi.status() != WL_CONNECTED) {
    showStatus("WLAN fehlgeschlagen!");
    updateRotatingText();
    return;
  }

  showStatus("Hole Uhrzeit...");

  // Mitteleuropa, automatischer Sommerzeit-Wechsel
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    timeReady = true;
  }

  tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
  updateRotatingText();
}

void loop() {
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

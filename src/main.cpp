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

// ---------- WLAN-Zugangsdaten (aus info.txt) ----------
String wifiSSID;
String wifiPASS;

// ---------- Liste der rotierenden Texte ----------
// Hier beliebig viele eigene Texte eintragen/aendern
const char* rotatingTexts[] = {
  "Willkommen!",
  "DarkFox Station",
  "Naechster Zug: puenktlich",
  "Habt einen schoenen Tag",
  "Powered by ESP32"
};
const int rotatingTextCount = sizeof(rotatingTexts) / sizeof(rotatingTexts[0]);
int currentTextIndex = 0;
unsigned long lastTextSwitch = 0;
const unsigned long TEXT_SWITCH_MS = 4000;

unsigned long lastClockUpdate = 0;
const unsigned long CLOCK_UPDATE_MS = 1000;

bool timeReady = false;

// ---------- SD: info.txt lesen ----------
bool loadWifiCredentials() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden");
    return false;
  }

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

// ---------- Bildschirm-Layout ----------
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
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);
  tft.drawString(rotatingTexts[currentTextIndex], tft.width() / 2, 180);
}

void setup() {
  Serial.begin(115200);

  // Backlight
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  // Display
  tft.init();
  tft.setRotation(1);

  drawStaticLayout();
  showStatus("Lese SD-Karte...");

  if (!loadWifiCredentials()) {
    showStatus("Fehler: SD/info.txt!");
    return;
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

  if (millis() - lastTextSwitch > TEXT_SWITCH_MS) {
    lastTextSwitch = millis();
    currentTextIndex = (currentTextIndex + 1) % rotatingTextCount;
    updateRotatingText();
  }
}

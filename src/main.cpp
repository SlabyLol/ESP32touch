#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <qrcode.h>
#include <XPT2046_Touchscreen.h>

// ---------- Display ----------
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL_PIN 27

// ---------- Touch (eigener SPI-Bus, getrennt vom Display!) ----------
#define TOUCH_SCK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TOUCH_CS   33
#define TOUCH_IRQ  36

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ---------- WiFi Portal ----------
const char* AP_SSID = "DarkFox-Setup";
const char* AP_PASS = "";  // offenes Netz; hier ein Passwort (min. 8 Zeichen) eintragen falls gewuenscht

WebServer server(80);

// ---------- Zustandsmaschine ----------
enum AppState { IDLE, PORTAL_ACTIVE, QR_SHOWN };
AppState state = IDLE;

unsigned long qrShownAt = 0;
const unsigned long QR_DISPLAY_MS = 10000;

unsigned long lastTouchCheck = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 400;

// ---------- Screens ----------
void showDarkFoxScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.drawString("DarkFox", tft.width() / 2, tft.height() / 2);
}

void showPortalScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Mit WLAN verbinden:", tft.width() / 2, 60);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(AP_SSID, tft.width() / 2, 90);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Dann im Browser oeffnen:", tft.width() / 2, 150);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(WiFi.softAPIP().toString(), tft.width() / 2, 180);
}

void showQRCode(const String &text) {
  QRCode qrcode;
  // Version 6 reicht fuer ca. 100+ Zeichen bei niedriger Fehlerkorrektur
  uint8_t qrcodeData[qrcode_getBufferSize(6)];
  qrcode_initText(&qrcode, qrcodeData, 6, ECC_LOW, text.c_str());

  tft.fillScreen(TFT_BLACK);

  int scale = min(tft.width(), tft.height()) / qrcode.size;
  if (scale < 1) scale = 1;
  int offsetX = (tft.width() - qrcode.size * scale) / 2;
  int offsetY = (tft.height() - qrcode.size * scale) / 2;

  tft.fillRect(offsetX - 8, offsetY - 8, qrcode.size * scale + 16, qrcode.size * scale + 16, TFT_WHITE);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
      tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, color);
    }
  }

  state = QR_SHOWN;
  qrShownAt = millis();
}

// ---------- Webserver Handler ----------
void handleRoot() {
  String html =
    "<html><body style='font-family:sans-serif;text-align:center;padding-top:50px;'>"
    "<h2>DarkFox QR Generator</h2>"
    "<form action='/generate' method='POST'>"
    "<input type='text' name='text' style='font-size:20px;padding:8px;width:80%;' autofocus>"
    "<br><br><input type='submit' value='QR-Code anzeigen' style='font-size:20px;padding:10px 20px;'>"
    "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleGenerate() {
  if (server.hasArg("text") && server.arg("text").length() > 0) {
    String text = server.arg("text");
    server.send(200, "text/html",
      "<html><body style='font-family:sans-serif;text-align:center;padding-top:50px;'>"
      "<h2>QR-Code wird angezeigt!</h2>"
      "<p>Verschwindet automatisch nach 10 Sekunden.</p></body></html>");
    showQRCode(text);
  } else {
    server.send(400, "text/plain", "Kein Text erhalten");
  }
}

void startWebPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, strlen(AP_PASS) > 0 ? AP_PASS : NULL);

  server.on("/", handleRoot);
  server.on("/generate", HTTP_POST, handleGenerate);
  server.begin();

  state = PORTAL_ACTIVE;
  showPortalScreen();
}

void stopWebPortal() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setup() {
  Serial.begin(115200);

  // Backlight
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  // Display
  tft.init();
  tft.setRotation(1);

  // Touch (eigener SPI-Bus, VSPI = globales SPI-Objekt, da TFT auf HSPI laeuft)
  SPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);

  showDarkFoxScreen();
}

void loop() {
  if (state == PORTAL_ACTIVE) {
    server.handleClient();
  }

  if (state == QR_SHOWN) {
    if (millis() - qrShownAt > QR_DISPLAY_MS) {
      stopWebPortal();
      state = IDLE;
      showDarkFoxScreen();
    }
  }

  // Touch nur im IDLE-Zustand auswerten, um das Portal zu starten
  if (state == IDLE && millis() - lastTouchCheck > TOUCH_DEBOUNCE_MS) {
    if (ts.touched()) {
      lastTouchCheck = millis();
      startWebPortal();
    }
  }
}

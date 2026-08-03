#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL_PIN 21   // Backlight-Pin beim CYD

void setup() {
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);

  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;

  tft.drawString("DarkFox", centerX, centerY);
}

void loop() {
}

// Minimal-Test ESP32-2432S032 – nur Display
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("DISPLAY TEST 2432S032");

  // BL GPIO 27 + Fallback 21
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(1);

  tft.fillScreen(TFT_RED);
  delay(800);
  tft.fillScreen(TFT_GREEN);
  delay(800);
  tft.fillScreen(TFT_BLUE);
  delay(800);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 40);
  tft.println("DarkFox OK");
  tft.setCursor(20, 80);
  tft.println("ST7789 BL27");
  tft.setCursor(20, 120);
  tft.println("Wenn du das");
  tft.setCursor(20, 150);
  tft.println("siehst: Display OK");

  Serial.println("draw done");
}

void loop() {
  delay(1000);
}

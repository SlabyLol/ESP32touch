#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define TOUCH_CS   33
#define TOUCH_IRQ  36

TFT_eSPI tft;
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Touch Test", 80, 10, 2);

    ts.begin();
    ts.setRotation(1);
}

void loop() {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();

        Serial.printf("Raw: X=%d Y=%d Z=%d\n",
                      p.x, p.y, p.z);

        // Raw values (adjust after calibration)
        int x = map(p.x, 300, 3800, 0, 320);
        int y = map(p.y, 300, 3800, 0, 240);

        x = constrain(x, 0, 319);
        y = constrain(y, 0, 239);

        tft.fillCircle(x, y, 3, TFT_RED);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("                ", 0, 220, 2);
        tft.drawString(String(x) + "," + String(y), 0, 220, 2);

        delay(20);
    }
}

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

uint16_t hbParseColor(String s) {
  s.trim();
  s.toUpperCase();
  if (s == "RED") return TFT_RED;
  if (s == "GREEN") return TFT_GREEN;
  if (s == "BLUE") return TFT_BLUE;
  if (s == "WHITE") return TFT_WHITE;
  if (s == "BLACK") return TFT_BLACK;
  if (s == "YELLOW") return TFT_YELLOW;
  if (s == "CYAN") return TFT_CYAN;
  if (s == "ORANGE") return TFT_ORANGE;
  if (s == "MAGENTA") return TFT_MAGENTA;
  if (s == "NAVY") return TFT_NAVY;
  if (s.length() == 6) {
    char *end;
    long v = strtol(s.c_str(), &end, 16);
    if (*end == 0) {
      uint8_t r = (v >> 16) & 0xFF;
      uint8_t g = (v >> 8) & 0xFF;
      uint8_t b = v & 0xFF;
      return tft.color565(r, g, b);
    }
  }
  return TFT_WHITE;
}

void hbWaitTouch() {
  delay(200);
  uint16_t tx, ty;
  while (true) {
    if (tft.getTouch(&tx, &ty)) {
      delay(200);
      return;
    }
    delay(20);
  }
}

bool loadHbPayload(const String& path, String& outScript) {
  File f = SD.open(path);
  if (!f) return false;
  f.seek(6);
  uint8_t tl[2];
  if (f.read(tl, 2) != 2) { f.close(); return false; }
  uint16_t titleLen = (uint16_t)tl[0] | ((uint16_t)tl[1] << 8);
  f.seek(8 + titleLen);
  uint8_t sz[4];
  if (f.read(sz, 4) != 4) { f.close(); return false; }
  uint32_t payloadSize = (uint32_t)sz[0] | ((uint32_t)sz[1] << 8) | ((uint32_t)sz[2] << 16) | ((uint32_t)sz[3] << 24);
  if (payloadSize > 3000) payloadSize = 3000;
  uint8_t* buf = (uint8_t*)malloc(payloadSize + 1);
  if (!buf) { f.close(); return false; }
  int n = f.read(buf, payloadSize);
  f.close();
  if (n <= 0) { free(buf); return false; }
  hbCrypt(buf, (size_t)n, titleLen);
  buf[n] = 0;
  outScript = String((char*)buf);
  free(buf);
  return true;
}

void hbShowWifiStatus(uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  if (WiFi.status() != WL_CONNECTED) {
    tft.drawString("WLAN: offline", 10, 50);
    return;
  }
  tft.drawString("WLAN: online", 10, 50);
  tft.drawString(WiFi.SSID(), 10, 80);
  tft.drawString(WiFi.localIP().toString(), 10, 110);
  char rssiBuf[24];
  snprintf(rssiBuf, sizeof(rssiBuf), "RSSI: %d dBm", WiFi.RSSI());
  tft.drawString(rssiBuf, 10, 140);
}

void hbHttpGet(const String& url, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);

  if (WiFi.status() != WL_CONNECTED) {
    tft.drawString("Kein WLAN", 10, 50);
    return;
  }

  tft.drawString("HTTP GET...", 10, 50);

  HTTPClient http;
  http.setTimeout(8000);
  bool ok = false;
  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    ok = http.begin(client, url);
  } else {
    ok = http.begin(url);
  }
  if (!ok) {
    tft.drawString("URL Fehler", 10, 80);
    return;
  }

  int code = http.GET();
  char codeBuf[32];
  snprintf(codeBuf, sizeof(codeBuf), "Status: %d", code);
  tft.drawString(codeBuf, 10, 80);

  if (code > 0) {
    String body = http.getString();
    body.replace('\r', ' ');
    body.replace('\n', ' ');
    if (body.length() > 120) body = body.substring(0, 120) + "...";
    tft.drawString(body, 10, 120);
  } else {
    tft.drawString("Request failed", 10, 120);
  }
  http.end();
}

void runHomeBrewScript(const String& raw) {
  String script = raw;
  int sep = script.indexOf("\n---\n");
  if (sep < 0) sep = script.indexOf("\n---\r\n");
  if (sep >= 0) script = script.substring(sep + 5);
  while (script.startsWith("\n") || script.startsWith("\r")) script = script.substring(1);

  uint16_t color = TFT_WHITE;
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);

  int start = 0;
  while (start < (int)script.length()) {
    int end = script.indexOf('\n', start);
    if (end < 0) end = script.length();
    String line = script.substring(start, end);
    start = end + 1;
    line.replace("\r", "");
    line.trim();
    if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) continue;

    if (line.equalsIgnoreCase("CLEAR")) {
      tft.fillScreen(TFT_BLACK);
      continue;
    }
    if (line.equalsIgnoreCase("WAIT_TOUCH") || line.equalsIgnoreCase("BTN")) {
      hbWaitTouch();
      continue;
    }
    if (line.startsWith("WAIT ") || line.startsWith("wait ")) {
      int ms = line.substring(5).toInt();
      if (ms < 0) ms = 0;
      if (ms > 15000) ms = 15000;
      delay(ms);
      continue;
    }
    if (line.startsWith("COLOR ") || line.startsWith("color ")) {
      color = hbParseColor(line.substring(6));
      continue;
    }
    if (line.equalsIgnoreCase("WIFI") || line.equalsIgnoreCase("WIFI_STATUS")) {
      hbShowWifiStatus(color);
      continue;
    }
    if (line.startsWith("HTTP ") || line.startsWith("http ")) {
      String url = line.substring(5);
      url.trim();
      hbHttpGet(url, color);
      continue;
    }
    if (line.startsWith("CENTER ") || line.startsWith("center ")) {
      String rest = line.substring(7);
      rest.trim();
      int sp = rest.indexOf(' ');
      if (sp > 0) {
        int y = rest.substring(0, sp).toInt();
        String msg = rest.substring(sp + 1);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);
        tft.drawString(msg, tft.width() / 2, y);
      }
      continue;
    }
    if (line.startsWith("TEXT ") || line.startsWith("text ")) {
      String rest = line.substring(5);
      rest.trim();
      int sp1 = rest.indexOf(' ');
      if (sp1 > 0) {
        int x = rest.substring(0, sp1).toInt();
        String rest2 = rest.substring(sp1 + 1);
        int sp2 = rest2.indexOf(' ');
        if (sp2 > 0) {
          int y = rest2.substring(0, sp2).toInt();
          String msg = rest2.substring(sp2 + 1);
          tft.setTextDatum(TL_DATUM);
          tft.setTextColor(color, TFT_BLACK);
          tft.drawString(msg, x, y);
        }
      }
      continue;
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(line, tft.width() / 2, 120);
  }
}

void showHbOpenScreen(int idx) {
  HbEntry &e = hbList[idx];
  String script;
  if (!loadHbPayload(e.path, script)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString("Load Fehler", tft.width() / 2, 120);
    delay(2000);
    drawHbMenu();
    return;
  }
  Serial.printf("HomeBrew RUN: %s\n", e.title.c_str());
  runHomeBrewScript(script);
  drawHbMenu();
}

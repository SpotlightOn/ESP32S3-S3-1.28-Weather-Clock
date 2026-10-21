// Minimal GC9A01 / TFT_eSPI test for the Spotpear ESP32-S3 1.28" BOX board.
//
// Prints the chip configuration, initialises the panel and cycles through
// solid colours with a readable label. Every step is reported on Serial so a
// black screen can be traced to either the panel, the SPI bus or the app.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// Backlight of this board is active low (GPIO42 low = backlight on)
static const int PIN_BACKLIGHT = 42;

TFT_eSPI tft;

static void logLine(const char *msg) {
  Serial.printf("[test] %s\n", msg);
  Serial.flush();
}

void setup() {
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);

  Serial.begin(115200);
  delay(1500);  // let the USB CDC device enumerate before the first output
  Serial.println();
  logLine("display test start");

  Serial.printf("[test] TFT_eSPI version : %s\n", TFT_ESPI_VERSION);
  Serial.printf("[test] chip             : %s rev %d, %d core(s)\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("[test] cpu              : %lu MHz\n", getCpuFrequencyMhz());
  Serial.printf("[test] flash            : %lu bytes, mode %d\n",
                (unsigned long)ESP.getFlashChipSize(), (int)ESP.getFlashChipMode());
  Serial.printf("[test] psram            : %lu bytes\n",
                (unsigned long)ESP.getPsramSize());
  Serial.printf("[test] sdk              : %s\n", ESP.getSdkVersion());
  Serial.flush();

  tft.init();
  logLine("tft.init() returned");

  tft.setRotation(4);
  Serial.printf("[test] panel            : %d x %d\n", tft.width(), tft.height());
  Serial.flush();

  const uint16_t colours[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  const char *names[4] = {"red", "green", "blue", "white"};

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(colours[i]);
    tft.setTextColor((i == 3) ? TFT_BLACK : TFT_WHITE, colours[i]);
    tft.setCursor(25, 100, 4);
    tft.printf("%s", names[i]);
    tft.setCursor(25, 130, 2);
    tft.printf("%d x %d", tft.width(), tft.height());
    Serial.printf("[test] filled %s\n", names[i]);
    Serial.flush();
    delay(1500);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(45, 105, 4);
  tft.println("display OK");
  logLine("setup finished");
}

void loop() {
  static uint32_t last = 0;
  static uint32_t dots = 0;

  if (millis() - last > 2000) {
    last = millis();
    dots++;
    Serial.printf("[test] alive %lu s, free heap %lu bytes\n",
                  (unsigned long)(millis() / 1000),
                  (unsigned long)ESP.getFreeHeap());
    Serial.flush();

    // Draw one pixel per cycle so a frozen panel is visible too
    tft.drawPixel((dots * 7) % 240, (dots * 11) % 240, TFT_CYAN);
  }
}

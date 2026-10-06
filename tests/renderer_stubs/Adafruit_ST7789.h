#pragma once
#include <cstdint>
class Adafruit_ST7789 {
 public:
  Adafruit_ST7789(int, int, int) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawRGBBitmap(int, int, uint16_t*, int, int) {}
  void init(int, int) {}
  void setRotation(int) {}
};

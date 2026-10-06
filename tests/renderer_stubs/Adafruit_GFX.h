#pragma once
#include <cstdint>
class GFXcanvas16 {
 public:
  GFXcanvas16(int, int) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawRoundRect(int, int, int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void setTextColor(uint16_t) {}
  void setTextSize(int) {}
  void setCursor(int, int) {}
  void print(const char*) {}
  uint16_t* getBuffer() { return nullptr; }
};

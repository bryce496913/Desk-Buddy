#pragma once
#include <cstdint>
class GFXcanvas16 {
 public:
  GFXcanvas16(int, int) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawRoundRect(int, int, int, int, int, uint16_t) {}
#ifdef DESK_BUDDY_TEST_DRAW_TRACE
  struct Circle { int x, y, radius; };
  Circle circles[8]{};
  int circleCount = 0;
  void fillCircle(int x, int y, int radius, uint16_t) {
    if (circleCount < 8) circles[circleCount++] = {x, y, radius};
  }
#else
  void fillCircle(int, int, int, uint16_t) {}
#endif
  void setTextColor(uint16_t) {}
  void setTextSize(int) {}
  void setCursor(int, int) {}
  void print(const char*) {}
  uint16_t* getBuffer() { return nullptr; }
};

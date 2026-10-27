#pragma once

#include <stdint.h>

enum class TouchGesture : uint8_t {
  None,
  Tap,
  Hold
};

enum class ButtonGesture : uint8_t {
  None,
  ShortPress,
  LongPress
};

void beginInputs();
struct InputEvents {
  TouchGesture touch;
  ButtonGesture button;
  // True while a touch that already reported Hold is still pressed (petting).
  bool touchHeld;
};

InputEvents updateInputs(uint32_t now);

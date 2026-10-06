#pragma once

#include <stdint.h>

enum class TouchGesture : uint8_t {
  None,
  Tap,
  Hold
};

void beginInputs();
struct InputEvents {
  TouchGesture touch;
  bool button;
};

InputEvents updateInputs(uint32_t now);

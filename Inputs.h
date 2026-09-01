#pragma once

#include "BehaviorEngine.h"

void beginInputs();
struct InputEvents {
  bool touch;
  bool button;
};

InputEvents updateInputs(uint32_t now);

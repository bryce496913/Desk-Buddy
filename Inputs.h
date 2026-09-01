#pragma once

#include "BehaviorEngine.h"

void beginInputs();
struct InputEvents {
  bool touch;
  bool button;
  BuddyEvent buttonEvent;
};

InputEvents updateInputs(uint32_t now, BuddyCoreState coreState);

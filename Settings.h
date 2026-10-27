#pragma once

#include <Arduino.h>
#include "BehaviorEngine.h"

// Small flash-backed preferences that survive power cycles.
void beginSettings();
BuddySoundMode loadSoundMode();
// Writes flash only when the mode differs from the stored one.
void saveSoundModeIfChanged(BuddySoundMode mode);

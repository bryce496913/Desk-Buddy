#pragma once

#include <Arduino.h>

#include "Config.h"

#if DESK_BUDDY_DIAGNOSTICS
void beginDiagnostics();
void updateDiagnostics(uint32_t now);
#endif

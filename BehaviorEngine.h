#pragma once

#include <Arduino.h>

#include "Config.h"

enum class BuddyCoreState : uint8_t { Awake, Sleeping };
enum class BuddyEvent : uint8_t {
  Touch,
  SoundDetected,
  ButtonPressed,
  IdleTimeout
};
enum class BuddyReaction : uint8_t { Idle, Generic };

#if DESK_BUDDY_DIAGNOSTICS
enum class DiagnosticReaction : uint8_t {
  Normal,
  Happy,
  Curious,
  Annoyed,
  Startled,
  Suspicious,
  Confused,
  Daydreaming
};
#endif

void beginBehaviorEngine(uint32_t now);
void updateBehaviorEngine(uint32_t now);
void processBuddyEvent(BuddyEvent event, uint32_t now);
BuddyCoreState getBuddyCoreState();
BuddyReaction getBuddyReaction();

#if DESK_BUDDY_DIAGNOSTICS
bool triggerDiagnosticReaction(DiagnosticReaction reaction, uint32_t now);
#endif

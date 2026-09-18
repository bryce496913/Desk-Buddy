#pragma once

#include <Arduino.h>
#include "BehaviorEngine.h"

enum class ReactionSound : uint8_t {
  None,
  Happy,
  Curious,
  Annoyed,
  Startled,
  Suspicious,
  Confused
};

void beginSoundEngine();
void playBootSound();
void playSleepSound();
void playWakeSound();
void startReactionSound(uint32_t now, ReactionSound sound);
#if DESK_BUDDY_DIAGNOSTICS
constexpr uint8_t DIAGNOSTIC_RANDOM_VARIANT = UINT8_MAX;
bool startDiagnosticReactionSound(uint32_t now, ReactionSound sound,
                                  uint8_t requestedVariantIndex,
                                  uint8_t &selectedVariantIndex);
#endif
void stopReactionSound();
void updateSoundEngine(uint32_t now, BuddyReaction reaction);
bool isSoundEngineActive();

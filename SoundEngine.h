#pragma once

#include <Arduino.h>
#include "BehaviorEngine.h"

enum class ReactionSound : uint8_t { Happy, Curious, Annoyed, Startled };

void beginSoundEngine();
void playBootSound();
void playSleepSound();
void playWakeSound();
void startReactionSound(uint32_t now, ReactionSound sound);
void stopReactionSound();
void updateSoundEngine(uint32_t now, BuddyReaction reaction);
bool isSoundEngineActive();

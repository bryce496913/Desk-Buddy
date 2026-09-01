#pragma once

#include <Arduino.h>
#include "BehaviorEngine.h"

void beginSoundEngine();
void playBootSound();
void playSleepSound();
void playWakeSound();
void startReactionSound(uint32_t now);
void stopReactionSound();
void updateSoundEngine(uint32_t now, BuddyReaction reaction);
bool isReactionAudioActive();

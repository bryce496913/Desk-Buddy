#pragma once

#include <Arduino.h>

enum class BuddyCoreState : uint8_t { Awake, Sleeping };
enum class BuddyEvent : uint8_t { Touch, SoundDetected, Wake, SleepRequested };
enum class BuddyReaction : uint8_t { Idle, Generic };

void beginBehaviorEngine(uint32_t now);
void updateBehaviorEngine(uint32_t now);
void processBuddyEvent(BuddyEvent event, uint32_t now);
BuddyCoreState getBuddyCoreState();
BuddyReaction getBuddyReaction();

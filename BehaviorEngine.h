#pragma once

#include <Arduino.h>

#include "Config.h"

enum class BuddyCoreState : uint8_t { Awake, Sleeping };
enum class BuddyEvent : uint8_t {
  TouchTap,
  TouchHold,
  SoundDetected,
  ButtonPressed,
  IdleTimeout
};
enum class BuddyReaction : uint8_t { Idle, Generic };

// Longer-lived disposition, independent of sleep state and transient reactions.
enum class BuddyMood : uint8_t {
  Calm,
  Engaged,
  Grumpy,
  Sleepy
};

#if DESK_BUDDY_DIAGNOSTICS
enum class DiagnosticAutonomousBehavior : uint8_t {
  Curious, Daydreaming, SideGlance, Bored, SuspiciousGlance,
  AnnoyedSquint, SleepyDrift, ExcitedScanning
};
bool triggerDiagnosticAutonomousBehavior(DiagnosticAutonomousBehavior behavior,
                                         uint32_t now);

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
enum class DiagnosticSoundVariant : uint8_t {
  Random,
  Variant1,
  Variant2,
  Variant3
};
#endif

void beginBehaviorEngine(uint32_t now);
void updateBehaviorEngine(uint32_t now);
void processBuddyEvent(BuddyEvent event, uint32_t now);
BuddyCoreState getBuddyCoreState();
BuddyReaction getBuddyReaction();
BuddyMood getBuddyMood();

#if DESK_BUDDY_DIAGNOSTICS
enum class DiagnosticRecentInteractionType : uint8_t {
  None, TouchTap, TouchHold, Sound
};
struct DiagnosticRecentInteractionContext {
  DiagnosticRecentInteractionType type;
  uint32_t ageMs;
  bool recent;
};
DiagnosticRecentInteractionContext getDiagnosticRecentInteractionContext(uint32_t now);

struct DiagnosticMoodState {
  BuddyMood mood;
  uint8_t engagementScore;
  uint8_t irritationScore;
  // Waking inactivity only; zero while physical sleep pauses the clock.
  uint32_t inactivityMs;
};

DiagnosticMoodState getDiagnosticMoodState(uint32_t now);
void setDiagnosticMood(BuddyMood mood, uint32_t now);
bool triggerDiagnosticReaction(DiagnosticReaction reaction,
                               DiagnosticSoundVariant variant, uint32_t now,
                               uint8_t &selectedVariantIndex);
#endif

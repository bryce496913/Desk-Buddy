#pragma once

#include "FaceRenderer.h"
#include "SoundEngine.h"

#if !DESK_BUDDY_DIAGNOSTICS
#error "BuddyLifeSimulator requires DESK_BUDDY_DIAGNOSTICS=1"
#endif

struct BuddyLifeSnapshot {
  uint32_t now;
  BuddyCoreState coreState;
  BuddyReaction reaction;
  BuddyMood mood;
  BuddySoundMode soundMode;
  uint8_t engagement;
  uint8_t irritation;
  uint32_t inactivityMs;
  DiagnosticRecentInteractionType recentInteraction;
  uint32_t recentInteractionAgeMs;
  bool recentInteractionValid;  // Within the production recent-context window.
  FaceExpression lastExpression;  // Last started reaction, retained after completion.
  ReactionSound lastSound;
  BuddyMood lastSoundMood;
  bool soundActive;
};

// BehaviorEngine and its stubs are process-global: use one simulation at a time.
// Link this implementation and actual BehaviorEngine.cpp, without hardware engines.
// Define DESK_BUDDY_TEST_LIFE_SIMULATOR=1 to reset autonomous history between runs.
class BuddyLifeSimulator {
 public:
  static constexpr uint32_t TICK_MS = 100;
  void reset(uint32_t startAt = 0);
  void advanceBy(uint32_t milliseconds);
  // Forward modular time: a numerically smaller destination advances through wrap.
  void advanceTo(uint32_t timestamp);
  void tap();
  void hold();
  void sound();
  void shortButtonPress();
  void longButtonPress();
  void autonomousEvent();
  uint32_t now() const;
  BuddyLifeSnapshot snapshot() const;
  // FIFO tickets normalized to each random call's range; empty queue returns minimum.
  void queueRandom(uint32_t value);
  void clearRandomQueue();
 private:
  void event(BuddyEvent value);
};

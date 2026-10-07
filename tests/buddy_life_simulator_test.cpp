#include <cassert>
#include <cstdint>
#include "BuddyLifeSimulator.h"

int main() {
  BuddyLifeSimulator sim;
  sim.reset();
  auto state = sim.snapshot();
  assert(state.now == 0 && state.coreState == BuddyCoreState::Awake);
  assert(state.reaction == BuddyReaction::Idle && state.mood == BuddyMood::Calm);
  assert(state.soundMode == BuddySoundMode::Normal);
  assert(state.engagement == 0 && state.irritation == 0 && state.inactivityMs == 0);
  assert(!state.recentInteractionValid && state.recentInteraction == DiagnosticRecentInteractionType::None);
  assert(state.lastExpression == FaceExpression::Normal && state.lastSound == ReactionSound::None);

  sim.tap();
  state = sim.snapshot();
  assert(state.reaction == BuddyReaction::Generic && state.lastExpression == FaceExpression::Happy);
  assert(state.engagement == 20 && state.lastSound == ReactionSound::Happy);
  assert(state.lastSoundMood == BuddyMood::Calm && state.soundActive);
  sim.advanceBy(599); assert(sim.snapshot().soundActive);
  sim.advanceBy(1); assert(!sim.snapshot().soundActive);
  sim.advanceTo(1799); assert(sim.snapshot().reaction == BuddyReaction::Generic);
  sim.advanceBy(1); assert(sim.snapshot().reaction == BuddyReaction::Idle);
  assert(sim.snapshot().lastExpression == FaceExpression::Happy);

  sim.reset(123);
  sim.advanceBy(600000);
  assert(sim.now() == 600123 && millis() == sim.now());
  assert(sim.snapshot().reaction == BuddyReaction::Idle && sim.snapshot().mood == BuddyMood::Sleepy);
  sim.reset(); sim.advanceTo(90550);
  assert(sim.now() == 90550 && sim.snapshot().inactivityMs == 90550);
  sim.advanceBy(0); assert(sim.now() == 90550);

  sim.reset(); sim.tap(); sim.hold();
  state = sim.snapshot();
  assert(state.engagement == 50 && state.mood == BuddyMood::Engaged);
  assert(state.recentInteraction == DiagnosticRecentInteractionType::TouchHold);
  assert(state.lastSoundMood == BuddyMood::Calm); // Arrival mood before crossing threshold.
  sim.sound(); state = sim.snapshot();
  assert(state.engagement == 55 && state.lastSoundMood == BuddyMood::Engaged);
  assert(state.recentInteraction == DiagnosticRecentInteractionType::Sound);
  sim.longButtonPress(); assert(sim.snapshot().soundMode == BuddySoundMode::Quiet);
  assert(!sim.snapshot().soundActive);
  sim.longButtonPress(); assert(sim.snapshot().soundMode == BuddySoundMode::Normal);
  sim.shortButtonPress(); assert(sim.snapshot().coreState == BuddyCoreState::Sleeping);
  sim.tap(); sim.hold(); sim.sound(); sim.autonomousEvent();
  assert(sim.snapshot().reaction == BuddyReaction::Idle);
  sim.advanceBy(100000); assert(sim.snapshot().engagement == 55);
  sim.longButtonPress(); assert(sim.snapshot().coreState == BuddyCoreState::Awake);
  assert(sim.snapshot().soundMode == BuddySoundMode::Normal);
  sim.advanceBy(600); sim.shortButtonPress(); sim.shortButtonPress();
  assert(sim.snapshot().coreState == BuddyCoreState::Awake);

  sim.reset(); sim.queueRandom(0); sim.autonomousEvent();
  state = sim.snapshot();
  assert(state.reaction == BuddyReaction::Generic && state.lastExpression == FaceExpression::Daydreaming);
  assert(state.lastSound == ReactionSound::None && !state.soundActive);
  sim.autonomousEvent(); assert(sim.snapshot().lastExpression == state.lastExpression); // Busy rejection.
  sim.advanceBy(1800);
  sim.queueRandom(25); sim.autonomousEvent();
  assert(sim.snapshot().lastExpression == FaceExpression::Curious); // Real no-repeat pool.
  sim.reset(); sim.autonomousEvent();
  assert(sim.snapshot().lastExpression == FaceExpression::Daydreaming); // Reset clears selection history.
  sim.queueRandom(17); sim.queueRandom(8);
  assert(random(10) == 7 && random(20, 25) == 23);
  sim.queueRandom(3); sim.clearRandomQueue();
  assert(random(100) == 0 && random(10, 20) == 10);
  sim.queueRandom(9); sim.reset(); assert(random(100) == 0);

  constexpr uint32_t start = UINT32_MAX - 500;
  sim.reset(start); sim.tap();
  sim.advanceBy(599); assert(sim.snapshot().soundActive);
  sim.advanceBy(1); assert(!sim.snapshot().soundActive);
  sim.advanceTo(static_cast<uint32_t>(start + 1799));
  assert(sim.snapshot().reaction == BuddyReaction::Generic);
  sim.advanceBy(1); assert(sim.snapshot().reaction == BuddyReaction::Idle);
  assert(sim.now() == static_cast<uint32_t>(start + 1800));
  assert(sim.snapshot().recentInteractionAgeMs == 1800);
  sim.advanceBy(100000);
  assert(sim.now() == static_cast<uint32_t>(start + 101800));
  assert(sim.snapshot().mood == BuddyMood::Sleepy && !sim.snapshot().recentInteractionValid);
}

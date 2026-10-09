#include <cassert>
#include <cstdio>
#include "BuddyLifeSimulator.h"

namespace {
BuddyLifeSnapshot checkedState(const BuddyLifeSimulator& buddy) {
  const auto state = buddy.snapshot();
  assert(state.engagement <= 100 && state.irritation <= 100);
  assert(state.mood == BuddyMood::Calm || state.mood == BuddyMood::Engaged ||
         state.mood == BuddyMood::Grumpy || state.mood == BuddyMood::Sleepy);
  assert(state.soundMode == BuddySoundMode::Normal || state.soundMode == BuddySoundMode::Quiet);
  assert(state.coreState == BuddyCoreState::Awake || state.coreState == BuddyCoreState::Sleeping);
  assert(state.reaction == BuddyReaction::Idle || state.reaction == BuddyReaction::Generic);
  assert(state.recentInteraction == DiagnosticRecentInteractionType::None ||
         state.recentInteraction == DiagnosticRecentInteractionType::TouchTap ||
         state.recentInteraction == DiagnosticRecentInteractionType::TouchHold ||
         state.recentInteraction == DiagnosticRecentInteractionType::Sound);
  if (state.recentInteraction == DiagnosticRecentInteractionType::None) {
    assert(!state.recentInteractionValid && state.recentInteractionAgeMs == 0);
  } else {
    assert(state.recentInteractionValid == (state.recentInteractionAgeMs <= 5000));
  }
  if (state.coreState == BuddyCoreState::Sleeping)
    assert(state.reaction == BuddyReaction::Idle && state.inactivityMs == 0);
  return state;
}

BuddyLifeSnapshot releaseLifecycle(bool quietTrial) {
  BuddyLifeSimulator buddy;
  buddy.reset();
  auto state = checkedState(buddy);
  assert(state.coreState == BuddyCoreState::Awake && state.reaction == BuddyReaction::Idle);
  assert(state.mood == BuddyMood::Calm && state.engagement == 0 && state.irritation == 0);
  assert(state.soundMode == BuddySoundMode::Normal);

  // t=100/200 friendly taps -> Engaged; t=300/400/500 excess -> Grumpy.
  buddy.advanceTo(100); buddy.tap();
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Calm && state.lastExpression == FaceExpression::Happy);
  buddy.advanceTo(200); buddy.tap();
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Engaged && state.engagement == 45);
  assert(state.lastExpression == FaceExpression::Curious && state.lastSoundMood == BuddyMood::Calm);
  for (uint32_t at : {300U,400U,500U}) {
    buddy.advanceTo(at); buddy.tap(); checkedState(buddy);
  }
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Grumpy && state.engagement == 60 && state.irritation == 75);

  // t=2300 Hold begins recovery without instantly erasing strong irritation.
  buddy.advanceTo(2300); buddy.hold();
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Grumpy && state.engagement == 90 && state.irritation == 60);
  assert(state.lastExpression == FaceExpression::Curious && state.lastSoundMood == BuddyMood::Grumpy);
  buddy.advanceTo(10000);
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Engaged && state.engagement == 85 && state.irritation == 50);

  // t=120000: decay eventually lowers engagement below precedence threshold.
  // Inactivity from the Hold is 117700ms; naturally Sleepy, physically Awake.
  buddy.advanceTo(120000);
  state = checkedState(buddy);
  assert(state.mood == BuddyMood::Sleepy && state.engagement == 30 && state.irritation == 0);
  assert(state.reaction == BuddyReaction::Idle && state.inactivityMs == 117700);
  buddy.sound();
  state = checkedState(buddy);
  assert(state.lastExpression == FaceExpression::Startled && state.lastSound == ReactionSound::Startled);
  assert(state.lastSoundMood == BuddyMood::Sleepy && state.soundActive);
  assert(state.mood == BuddyMood::Calm && state.engagement == 35 && state.inactivityMs == 0);

  // Quiet toggle stops the chirp without changing arrival context or personality.
  if (quietTrial) buddy.longButtonPress();
  buddy.advanceTo(120100); buddy.tap();
  const auto tap = checkedState(buddy);
  assert(tap.reaction == BuddyReaction::Generic && tap.lastExpression == FaceExpression::Curious);
  assert(tap.recentInteraction == DiagnosticRecentInteractionType::TouchTap);
  assert(tap.mood == BuddyMood::Engaged && tap.engagement == 55);
  assert(tap.soundActive == !quietTrial);
  assert(tap.soundMode == (quietTrial ? BuddySoundMode::Quiet : BuddySoundMode::Normal));
  if (quietTrial) {
    buddy.longButtonPress();
    state = checkedState(buddy);
    assert(state.soundMode == BuddySoundMode::Normal && !state.soundActive); // No retroactive chirp.
  }

  // t=120200 sleep interrupts the reaction; three minutes preserve disposition.
  buddy.advanceTo(120200); buddy.shortButtonPress();
  const auto sleeping = checkedState(buddy);
  assert(sleeping.coreState == BuddyCoreState::Sleeping && sleeping.reaction == BuddyReaction::Idle);
  buddy.tap(); buddy.hold(); buddy.sound(); buddy.autonomousEvent();
  state = checkedState(buddy);
  assert(state.engagement == sleeping.engagement && state.irritation == sleeping.irritation);
  assert(state.lastExpression == sleeping.lastExpression && state.lastSound == sleeping.lastSound);
  assert(state.recentInteraction == DiagnosticRecentInteractionType::None);
  buddy.advanceBy(180000);
  state = checkedState(buddy);
  assert(state.coreState == BuddyCoreState::Sleeping && state.engagement == 55);
  buddy.shortButtonPress();
  state = checkedState(buddy);
  assert(state.coreState == BuddyCoreState::Awake && state.mood == BuddyMood::Engaged);
  assert(state.engagement == 55 && state.inactivityMs == 0);

  // t=300800 wake audio has ended; ticket zero selects real Engaged autonomy.
  buddy.advanceBy(600);
  const auto beforeAuto = checkedState(buddy);
  assert(beforeAuto.reaction == BuddyReaction::Idle && !beforeAuto.soundActive);
  buddy.queueRandom(0); buddy.autonomousEvent();
  const auto autonomous = checkedState(buddy);
  assert(autonomous.reaction == BuddyReaction::Generic);
  assert(autonomous.lastExpression == FaceExpression::ExcitedScanning);
  assert(autonomous.lastSound == ReactionSound::None && !autonomous.soundActive);
  assert(autonomous.mood == beforeAuto.mood && autonomous.engagement == beforeAuto.engagement);
  assert(autonomous.irritation == beforeAuto.irritation && autonomous.inactivityMs == beforeAuto.inactivityMs);
  assert(autonomous.recentInteraction == beforeAuto.recentInteraction);
  assert(autonomous.recentInteractionAgeMs == beforeAuto.recentInteractionAgeMs);
  assert(autonomous.recentInteractionValid == beforeAuto.recentInteractionValid);
  buddy.advanceBy(1800);
  state = checkedState(buddy);
  assert(state.reaction == BuddyReaction::Idle && state.inactivityMs == 2400);
  assert(buddy.now() == 302600);
  return tap;
}
}
int main() {
  // Same naturally evolved arrival state in both runs; only the preference differs.
  const auto normal = releaseLifecycle(false);
  const auto quiet = releaseLifecycle(true);
  assert(normal.lastExpression == quiet.lastExpression && normal.reaction == quiet.reaction);
  assert(normal.mood == quiet.mood && normal.engagement == quiet.engagement);
  assert(normal.irritation == quiet.irritation && normal.inactivityMs == quiet.inactivityMs);
  assert(normal.recentInteraction == quiet.recentInteraction);
  assert(normal.recentInteractionAgeMs == quiet.recentInteractionAgeMs);
  assert(normal.recentInteractionValid == quiet.recentInteractionValid);
  std::puts("[PASS] V2 release lifecycle: natural moods, Quiet parity, sleep/wake, silent autonomy");
}

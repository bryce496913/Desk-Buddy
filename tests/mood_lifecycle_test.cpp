#include <cassert>
#include <cstdint>
#include <limits>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

static_assert(DESK_BUDDY_DIAGNOSTICS == 1,
              "lifecycle telemetry requires diagnostics mode");

namespace {
FaceExpression expression = FaceExpression::Normal;
ReactionSound sound = ReactionSound::None;
bool faceFinished = false;
bool soundActive = false;
int faceStarts = 0;
int soundStarts = 0;

void beginAt(uint32_t now = 0) {
  expression = FaceExpression::Normal;
  sound = ReactionSound::None;
  faceFinished = false;
  soundActive = false;
  faceStarts = 0;
  soundStarts = 0;
  beginBehaviorEngine(now);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}

void expectMood(uint32_t now, BuddyMood mood, uint8_t engagement,
                uint8_t irritation, uint32_t inactivity) {
  const DiagnosticMoodState state = getDiagnosticMoodState(now);
  assert(state.mood == mood);
  assert(getBuddyMood() == mood);
  assert(state.engagementScore == engagement);
  assert(state.irritationScore == irritation);
  assert(state.inactivityMs == inactivity);
}

void expectReaction(FaceExpression expectedExpression,
                    ReactionSound expectedSound) {
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(expression == expectedExpression);
  assert(sound == expectedSound);
}

void finishAt(uint32_t now) {
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(now);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}

void buildGrumpy() {
  for (uint32_t now = 100; now <= 104; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  expectMood(104, BuddyMood::Grumpy, 60, 75, 0);
}

void testFriendlyEngagementThenCalm() {
  beginAt();
  expectMood(0, BuddyMood::Calm, 0, 0, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  expectMood(100, BuddyMood::Calm, 20, 0, 0);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::TouchTap, 200);
  expectMood(200, BuddyMood::Engaged, 45, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  finishAt(201);
  updateBehaviorEngine(10000);
  expectMood(10000, BuddyMood::Engaged, 40, 0, 9800);
  updateBehaviorEngine(20000);
  expectMood(20000, BuddyMood::Calm, 35, 0, 19800);
}

void testTapAndHoldTemporaryEquivalence() {
  for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
    beginAt();
    processBuddyEvent(event, 100);
    expectReaction(FaceExpression::Happy, ReactionSound::Happy);
    expectMood(100, BuddyMood::Calm, 20, 0, 0);
    processBuddyEvent(event, 200);
    expectReaction(FaceExpression::Curious, ReactionSound::Curious);
    expectMood(200, BuddyMood::Engaged, 45, 0, 0);
    processBuddyEvent(event, 300);
    expectReaction(FaceExpression::Curious, ReactionSound::Curious);
    expectMood(300, BuddyMood::Engaged, 50, 25, 0);
    finishAt(301);
    expectMood(301, BuddyMood::Engaged, 50, 25, 1);
    processBuddyEvent(event, 6301);  // Same streak expiration for both events.
    expectReaction(FaceExpression::Happy, ReactionSound::Happy);
    expectMood(6301, BuddyMood::Engaged, 70, 25, 0);

    processBuddyEvent(BuddyEvent::ButtonPressed, 6400);
    const int facesBefore = faceStarts;
    const int soundsBefore = soundStarts;
    processBuddyEvent(event, 6500);
    assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
    assert(getBuddyReaction() == BuddyReaction::Idle);
    expectMood(6500, BuddyMood::Engaged, 70, 25, 0);
    assert(faceStarts == facesBefore && soundStarts == soundsBefore);
  }

  // Tap and Hold share the existing touch history, rather than separate streaks.
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchHold, 200);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(200, BuddyMood::Engaged, 45, 0, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 300);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(300, BuddyMood::Engaged, 50, 25, 0);
}

void testExcessiveAttentionAndRecovery() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  expectMood(101, BuddyMood::Engaged, 45, 0, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 102);
  expectMood(102, BuddyMood::Engaged, 50, 25, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 103);
  expectMood(103, BuddyMood::Engaged, 55, 50, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 104);
  expectMood(104, BuddyMood::Grumpy, 60, 75, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  finishAt(105);
  updateBehaviorEngine(10000);
  expectMood(10000, BuddyMood::Grumpy, 55, 65, 9896);
  updateBehaviorEngine(20000);
  expectMood(20000, BuddyMood::Engaged, 50, 55, 19896);
  updateBehaviorEngine(50000);
  expectMood(50000, BuddyMood::Calm, 35, 25, 49896);
}

void testInactivityThroughAutonomousReactions() {
  beginAt();
  // Diagnostics suppress scheduling; explicit IdleTimeout uses the same real
  // autonomous path. The production suite also covers scheduled autonomy.
  const uint32_t deadlines[] = {20000, 40000, 60000, 80000};
  for (uint32_t now : deadlines) {
    updateBehaviorEngine(now);
    processBuddyEvent(BuddyEvent::IdleTimeout, now);
    assert(expression == FaceExpression::Curious ||
           expression == FaceExpression::Daydreaming);
    expectMood(now, BuddyMood::Calm, 0, 0, now);
    finishAt(now + 1);
    expectMood(now + 1, BuddyMood::Calm, 0, 0, now + 1);
  }
  updateBehaviorEngine(89999);
  expectMood(89999, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(90000);
  expectMood(90000, BuddyMood::Sleepy, 0, 0, 90000);
}

void testSleepyTouchAndSoundAlertness() {
  beginAt();
  updateBehaviorEngine(90000);
  processBuddyEvent(BuddyEvent::TouchTap, 90001);
  expectMood(90001, BuddyMood::Calm, 20, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::TouchTap, 90002);
  expectMood(90002, BuddyMood::Engaged, 45, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  beginAt();
  updateBehaviorEngine(90000);
  expectMood(90000, BuddyMood::Sleepy, 0, 0, 90000);
  processBuddyEvent(BuddyEvent::SoundDetected, 90001);
  expectMood(90001, BuddyMood::Calm, 5, 0, 0);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
}

void testSoundRestartsInactivity() {
  beginAt();
  updateBehaviorEngine(89998);
  processBuddyEvent(BuddyEvent::SoundDetected, 89999);
  expectMood(89999, BuddyMood::Calm, 5, 0, 0);
  finishAt(90000);  // One decay tick removes the five sound points.
  expectMood(90000, BuddyMood::Calm, 0, 0, 1);
  updateBehaviorEngine(179998);
  expectMood(179998, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(179999);
  expectMood(179999, BuddyMood::Sleepy, 0, 0, 90000);
}

void testPhysicalSleepPausesScoresAndInactivity() {
  beginAt();
  buildGrumpy();
  processBuddyEvent(BuddyEvent::ButtonPressed, 200);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  constexpr uint32_t wakeAt = 8 * 60 * 60 * 1000;
  updateBehaviorEngine(wakeAt - 1);
  expectMood(wakeAt - 1, BuddyMood::Grumpy, 60, 75, 0);
  processBuddyEvent(BuddyEvent::ButtonPressed, wakeAt);
  updateBehaviorEngine(wakeAt);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  expectMood(wakeAt, BuddyMood::Grumpy, 60, 75, 0);
  updateBehaviorEngine(wakeAt + 10000);
  expectMood(wakeAt + 10000, BuddyMood::Grumpy, 55, 65, 10000);
  updateBehaviorEngine(wakeAt + 20000);
  expectMood(wakeAt + 20000, BuddyMood::Engaged, 50, 55, 20000);
}

void testRolloverForDecayAndInactivity() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  beginAt(anchor);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + 1);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + 2);
  finishAt(anchor + 3);
  updateBehaviorEngine(anchor + uint32_t{10000});
  expectMood(anchor + uint32_t{10000}, BuddyMood::Engaged, 40, 0, 9998);
  updateBehaviorEngine(anchor + uint32_t{20000});
  expectMood(anchor + uint32_t{20000}, BuddyMood::Calm, 35, 0, 19998);

  beginAt(anchor);
  updateBehaviorEngine(anchor + uint32_t{89999});
  expectMood(anchor + uint32_t{89999}, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(anchor + uint32_t{90000});
  expectMood(anchor + uint32_t{90000}, BuddyMood::Sleepy, 0, 0, 90000);
}

void testScoreBoundsAndReadOnlyTelemetry() {
  beginAt();
  for (uint32_t now = 1; now <= 1000; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
    const DiagnosticMoodState state = getDiagnosticMoodState(now);
    assert(state.engagementScore <= 100 && state.irritationScore <= 100);
  }
  expectMood(1000, BuddyMood::Grumpy, 100, 100, 0);
  const int facesBeforeRead = faceStarts;
  const int soundsBeforeRead = soundStarts;
  // Reading at a much later timestamp must not apply decay or mutate anchors.
  expectMood(1000000, BuddyMood::Grumpy, 100, 100, 999000);
  expectMood(1000, BuddyMood::Grumpy, 100, 100, 0);
  assert(faceStarts == facesBeforeRead && soundStarts == soundsBeforeRead);
  assert(getBuddyReaction() == BuddyReaction::Generic);

  beginAt();
  for (uint32_t now = 1; now <= 1000; ++now) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
    const DiagnosticMoodState state = getDiagnosticMoodState(now);
    assert(state.engagementScore <= 100 && state.irritationScore == 0);
  }
  expectMood(1000, BuddyMood::Engaged, 100, 0, 0);
}

void testGrumpyReactionContext() {
  beginAt();
  buildGrumpy();
  finishAt(105);
  processBuddyEvent(BuddyEvent::TouchTap, 6105);  // Streak expired, mood retained.
  expectMood(6105, BuddyMood::Grumpy, 80, 75, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
}

void testAutomaticMoodContextForIdenticalFirstSounds() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  finishAt(102);
  updateBehaviorEngine(6102);  // Touch streak expired, engagement retained.
  expectMood(6102, BuddyMood::Engaged, 45, 0, 6001);
  processBuddyEvent(BuddyEvent::SoundDetected, 6103);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(6103, BuddyMood::Engaged, 50, 0, 0);

  for (uint32_t now = 6104; now <= 6108; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  expectMood(6108, BuddyMood::Grumpy, 100, 75, 0);
  finishAt(6109);
  updateBehaviorEngine(16104);  // One decay tick; sound window expired.
  expectMood(16104, BuddyMood::Grumpy, 95, 65, 9996);
  processBuddyEvent(BuddyEvent::SoundDetected, 16104);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  expectMood(16104, BuddyMood::Grumpy, 100, 65, 0);
}
}  // namespace

long random(long) { return 0; }
long random(long minimum, long) { return minimum; }
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression requested) {
  expression = requested;
  faceFinished = false;
  ++faceStarts;
}
bool isFaceReactionFinished(uint32_t) { return faceFinished; }
void finishFaceReaction(uint32_t) {
  expression = FaceExpression::Normal;
  faceFinished = false;
}
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}
void startReactionSound(uint32_t, ReactionSound requested) {
  sound = requested;
  soundActive = sound != ReactionSound::None;
  ++soundStarts;
}
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requested,
                                  uint8_t, uint8_t &selected) {
  selected = requested == ReactionSound::None ? DIAGNOSTIC_RANDOM_VARIANT : 0;
  startReactionSound(now, requested);
  return true;
}
void stopReactionSound() {
  sound = ReactionSound::None;
  soundActive = false;
}
bool isSoundEngineActive() { return soundActive; }
void playSleepSound() {}
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}

int main() {
  testFriendlyEngagementThenCalm();
  testTapAndHoldTemporaryEquivalence();
  testExcessiveAttentionAndRecovery();
  testInactivityThroughAutonomousReactions();
  testSleepyTouchAndSoundAlertness();
  testSoundRestartsInactivity();
  testPhysicalSleepPausesScoresAndInactivity();
  testRolloverForDecayAndInactivity();
  testScoreBoundsAndReadOnlyTelemetry();
  testGrumpyReactionContext();
  testAutomaticMoodContextForIdenticalFirstSounds();
  return 0;
}

#include <cassert>
#include <cstdint>
#include <limits>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

static_assert(DESK_BUDDY_DIAGNOSTICS == 1,
              "mood state test requires diagnostic mood selection");

namespace {
constexpr BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged,
                               BuddyMood::Grumpy, BuddyMood::Sleepy};
FaceExpression expression = FaceExpression::Normal;
ReactionSound sound = ReactionSound::None;
bool faceFinished = false;
bool soundActive = false;
int faceStarts = 0;
int soundStarts = 0;
int faceFinishes = 0;

void expectState(BuddyCoreState core, BuddyReaction reaction, BuddyMood mood) {
  assert(getBuddyCoreState() == core);
  assert(getBuddyReaction() == reaction);
  assert(getBuddyMood() == mood);
}

void beginAt(uint32_t now = 100) {
  expression = FaceExpression::Normal;
  sound = ReactionSound::None;
  faceFinished = false;
  soundActive = false;
  faceStarts = 0;
  soundStarts = 0;
  faceFinishes = 0;
  beginBehaviorEngine(now);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Calm);
}

void expectReaction(FaceExpression expectedExpression, ReactionSound expectedSound,
                    BuddyMood mood) {
  expectState(BuddyCoreState::Awake, BuddyReaction::Generic, mood);
  assert(expression == expectedExpression);
  assert(sound == expectedSound);
}

void triggerDiagnostic(DiagnosticReaction reaction, uint32_t now) {
  uint8_t selectedVariant = DIAGNOSTIC_RANDOM_VARIANT;
  assert(triggerDiagnosticReaction(reaction, DiagnosticSoundVariant::Random,
                                   now, selectedVariant));
}

void testStartupAndAwakeIdleMoods() {
  beginAt();
  for (BuddyMood mood : moods) {
    setDiagnosticMood(mood, 100);
    expectState(BuddyCoreState::Awake, BuddyReaction::Idle, mood);
    assert(faceStarts == 0);
    assert(soundStarts == 0);
    assert(expression == FaceExpression::Normal);
    assert(sound == ReactionSound::None);
  }
}

void testTouchCoexistenceAndCompletion() {
  for (BuddyMood mood : moods) {
    beginAt();  // Fresh history ensures this is a first touch for every mood.
    setDiagnosticMood(mood, 100);
    processBuddyEvent(BuddyEvent::TouchTap, 110);
    const BuddyMood moodAfterTouch =
        mood == BuddyMood::Sleepy ? BuddyMood::Calm : mood;
    const bool curious = mood == BuddyMood::Grumpy || mood == BuddyMood::Sleepy;
    expectReaction(curious ? FaceExpression::Curious : FaceExpression::Happy,
                   curious ? ReactionSound::Curious : ReactionSound::Happy,
                   moodAfterTouch);
    updateBehaviorEngine(111);
    expectState(BuddyCoreState::Awake, BuddyReaction::Generic, moodAfterTouch);
    assert(faceFinishes == 0);

    faceFinished = true;
    soundActive = false;
    updateBehaviorEngine(112);
    expectState(BuddyCoreState::Awake, BuddyReaction::Idle, moodAfterTouch);
    assert(faceFinishes == 1);
    assert(expression == FaceExpression::Normal);
  }
}

void testTouchLadderPreservesMood() {
  beginAt();
  setDiagnosticMood(BuddyMood::Engaged, 100);
  const FaceExpression expressions[] = {FaceExpression::Happy,
                                        FaceExpression::Happy,
                                        FaceExpression::Curious};
  const ReactionSound sounds[] = {ReactionSound::Happy, ReactionSound::Happy,
                                  ReactionSound::Curious};
  for (uint8_t index = 0; index < 3; ++index) {
    processBuddyEvent(BuddyEvent::TouchTap, 110 + index);
    expectReaction(expressions[index], sounds[index], BuddyMood::Engaged);
  }
}

void testSoundLadderAlertsSleepy() {
  beginAt();
  setDiagnosticMood(BuddyMood::Sleepy, 100);
  const FaceExpression expressions[] = {FaceExpression::Startled,
                                        FaceExpression::Suspicious,
                                        FaceExpression::Confused};
  const ReactionSound sounds[] = {ReactionSound::Startled,
                                  ReactionSound::Suspicious,
                                  ReactionSound::Confused};
  for (uint8_t index = 0; index < 3; ++index) {
    processBuddyEvent(BuddyEvent::SoundDetected, 110 + index);
    expectReaction(expressions[index], sounds[index], BuddyMood::Calm);
  }
}

void testDiagnosticPersonalitiesPreserveMood() {
  for (BuddyMood mood : moods) {
    beginAt();
    setDiagnosticMood(mood, 100);
    // Diagnostics suppress scheduled autonomy; exercise its expressions through
    // the existing diagnostic path without changing production scheduling.
    triggerDiagnostic(DiagnosticReaction::Curious, 110);
    expectReaction(FaceExpression::Curious, ReactionSound::Curious, mood);
    triggerDiagnostic(DiagnosticReaction::Daydreaming, 120);
    expectReaction(FaceExpression::Daydreaming, ReactionSound::None, mood);
  }
}

void testSleepWakePreservesEveryMood() {
  for (BuddyMood mood : moods) {
    beginAt();
    setDiagnosticMood(mood, 100);
    processBuddyEvent(BuddyEvent::ButtonPressed, 110);
    expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, mood);
    processBuddyEvent(BuddyEvent::TouchTap, 120);
    processBuddyEvent(BuddyEvent::SoundDetected, 130);
    updateBehaviorEngine(140);
    expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, mood);
    assert(faceStarts == 0);
    processBuddyEvent(BuddyEvent::ButtonPressed, 150);
    const BuddyMood wakeMood = mood == BuddyMood::Sleepy ? BuddyMood::Calm : mood;
    expectState(BuddyCoreState::Awake, BuddyReaction::Idle, wakeMood);
  }
}

void testDiagnosticNormalPreservesMood() {
  beginAt();
  setDiagnosticMood(BuddyMood::Engaged, 100);
  triggerDiagnostic(DiagnosticReaction::Annoyed, 110);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed,
                 BuddyMood::Engaged);
  triggerDiagnostic(DiagnosticReaction::Normal, 120);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Engaged);
  assert(expression == FaceExpression::Normal);
  assert(sound == ReactionSound::None);
}

void testMoodChangesPreserveHistories() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 6110);  // Original touch window boundary.
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed,
                 BuddyMood::Grumpy);

  processBuddyEvent(BuddyEvent::SoundDetected, 6120);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious,
                 BuddyMood::Grumpy);
  setDiagnosticMood(BuddyMood::Sleepy, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 16120);  // Sound window boundary.
  expectReaction(FaceExpression::Startled, ReactionSound::Startled,
                 BuddyMood::Calm);
}

void testReinitializationResetsMood() {
  beginAt();
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::ButtonPressed, 110);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Grumpy);
  beginBehaviorEngine(200);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 210);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 220);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);
}

void testFreshTouchProgressionAndSaturation() {
  beginAt();
  const BuddyMood expectedMoods[] = {BuddyMood::Calm, BuddyMood::Engaged,
                                     BuddyMood::Engaged, BuddyMood::Engaged,
                                     BuddyMood::Grumpy};
  const FaceExpression expressions[] = {
      FaceExpression::Happy, FaceExpression::Curious, FaceExpression::Curious,
      FaceExpression::Curious, FaceExpression::Curious};
  const ReactionSound sounds[] = {
      ReactionSound::Happy, ReactionSound::Curious, ReactionSound::Curious,
      ReactionSound::Curious, ReactionSound::Curious};
  for (uint8_t index = 0; index < 5; ++index) {
    processBuddyEvent(BuddyEvent::TouchTap, 110 + index);
    expectReaction(expressions[index], sounds[index], expectedMoods[index]);
  }
  // Many increments would wrap either uint8_t score without saturation.
  // Both scores become high; irritation must continue to take precedence.
  for (uint32_t now = 115; now < 1115; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
    expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed,
                   BuddyMood::Grumpy);
  }
}

void testExpiredTouchWindowPreservesScores() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 6111);
  // Two separate first touches contribute 20 + 20, reaching exactly 40.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::TouchTap, 12112);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);

  beginAt();
  for (uint32_t now = 110; now < 115; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  processBuddyEvent(BuddyEvent::TouchTap, 6115);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Grumpy);
}

void testSoundAndIdleEventsPreserveAccumulatedMood() {
  const int touchCounts[] = {2, 5};
  for (int touchCount : touchCounts) {
    beginAt();
    for (int index = 0; index < touchCount; ++index) {
      processBuddyEvent(BuddyEvent::TouchTap, 110 + index);
    }
    const BuddyMood mood = touchCount == 2 ? BuddyMood::Engaged : BuddyMood::Grumpy;
    processBuddyEvent(BuddyEvent::SoundDetected, 120);
    expectReaction(mood == BuddyMood::Engaged ? FaceExpression::Curious : FaceExpression::Suspicious,
                   mood == BuddyMood::Engaged ? ReactionSound::Curious : ReactionSound::Suspicious, mood);
    processBuddyEvent(BuddyEvent::SoundDetected, 121);
    expectReaction(mood == BuddyMood::Grumpy ? FaceExpression::Annoyed : FaceExpression::Suspicious,
                   mood == BuddyMood::Grumpy ? ReactionSound::Annoyed : ReactionSound::Suspicious, mood);
    processBuddyEvent(BuddyEvent::SoundDetected, 122);
    expectReaction(mood == BuddyMood::Grumpy ? FaceExpression::Annoyed : FaceExpression::Confused,
                   mood == BuddyMood::Grumpy ? ReactionSound::Annoyed : ReactionSound::Confused, mood);

    faceFinished = true;
    soundActive = false;
    updateBehaviorEngine(130);
    // Scheduled autonomy remains suppressed in diagnostics; explicit IdleTimeout
    // exercises the actual autonomous path without changing that suppression.
    processBuddyEvent(BuddyEvent::IdleTimeout, 140);
    expectState(BuddyCoreState::Awake, BuddyReaction::Generic, mood);
    assert(expression == FaceExpression::Curious ||
           expression == FaceExpression::Daydreaming);
    assert(sound == ReactionSound::None);
    faceFinished = true;
    updateBehaviorEngine(150);
    // Sound and autonomy do not reset the existing decay clock.
    updateBehaviorEngine(9999);
    expectState(BuddyCoreState::Awake, BuddyReaction::Idle, mood);
    processBuddyEvent(BuddyEvent::TouchTap, 10000);
    expectReaction(mood == BuddyMood::Grumpy ? FaceExpression::Curious : FaceExpression::Happy,
                   mood == BuddyMood::Grumpy ? ReactionSound::Curious : ReactionSound::Happy, mood);
  }
}

void testSleepWakePreservesAccumulatedScores() {
  // A subthreshold engagement score survives sleep and combines with a new
  // first touch after wake (20 + 20), rather than starting from zero again.
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 110);
  processBuddyEvent(BuddyEvent::ButtonPressed, 120);
  updateBehaviorEngine(1000000);
  processBuddyEvent(BuddyEvent::ButtonPressed, 1000010);
  processBuddyEvent(BuddyEvent::TouchTap, 1000020);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);

  // Four touches leave irritation at 50. Sleep resets the reaction streak,
  // but the third touch after wake must add 25 to that retained irritation.
  beginAt();
  for (uint32_t now = 110; now < 114; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  processBuddyEvent(BuddyEvent::ButtonPressed, 120);
  processBuddyEvent(BuddyEvent::ButtonPressed, 130);
  processBuddyEvent(BuddyEvent::TouchTap, 140);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::TouchTap, 150);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy,
                 BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::TouchTap, 160);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, 170);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, 180);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Grumpy);
}

void testDiagnosticCanonicalScores() {
  beginAt();
  for (uint32_t now = 110; now < 120; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  setDiagnosticMood(BuddyMood::Calm, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 6120);  // Fresh streak, cleared scores.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 6121);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);

  setDiagnosticMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 12122);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 18123);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Grumpy);

  setDiagnosticMood(BuddyMood::Sleepy, 18123);
  updateBehaviorEngine(18124);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::SoundDetected, 18125);
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 24124);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 24125);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);
}

void buildTouchMood(uint32_t start, int count) {
  for (int index = 0; index < count; ++index) {
    processBuddyEvent(BuddyEvent::TouchTap, start + index);
  }
}

void testEngagedDecayBoundariesAndRemainder() {
  beginAt(0);
  buildTouchMood(1, 2);  // engagement 45, irritation 0.
  updateBehaviorEngine(9999);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(10000);  // 40 remains Engaged.
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(10000);  // Repeating an update must not decay twice.
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(19999);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(20000);  // 35 becomes Calm.
  assert(getBuddyMood() == BuddyMood::Calm);

  beginAt(0);
  buildTouchMood(1, 2);
  updateBehaviorEngine(15000);  // One step, with a 5000 ms remainder.
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(19999);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(20000);
  assert(getBuddyMood() == BuddyMood::Calm);
}

void testGrumpyRecoveryAndMultipleTicks() {
  beginAt(0);
  buildTouchMood(1, 5);  // engagement 60, irritation 75.
  updateBehaviorEngine(10000);  // 55/65.
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(20000);  // 50/55.
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(40000);  // 40/35.
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(50000);  // 35/25.
  assert(getBuddyMood() == BuddyMood::Calm);

  beginAt(0);
  buildTouchMood(1, 5);
  updateBehaviorEngine(50000);  // All five steps in a single update.
  assert(getBuddyMood() == BuddyMood::Calm);
  // A new first touch adds 20 to the retained engagement 35, proving this
  // was score decay rather than a blanket reset to zero.
  processBuddyEvent(BuddyEvent::TouchTap, 50001);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
}

void testLongGapSaturatesAtZero() {
  constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
  beginAt(0);
  updateBehaviorEngine(max);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::TouchTap, max);
  assert(getBuddyMood() == BuddyMood::Calm);  // Zero + 20, no underflow.

  beginAt(0);
  buildTouchMood(1, 100);  // Both scores saturate at 100.
  updateBehaviorEngine(max);
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, max);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
}

void testSleepPausesDecayClock() {
  beginAt(0);
  buildTouchMood(1, 5);
  processBuddyEvent(BuddyEvent::ButtonPressed, 9999);
  constexpr uint32_t wakeAt = 8 * 60 * 60 * 1000;
  updateBehaviorEngine(wakeAt - 1);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, wakeAt);
  updateBehaviorEngine(wakeAt);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Grumpy);
  updateBehaviorEngine(wakeAt + 9999);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(wakeAt + 10000);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(wakeAt + 20000);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(wakeAt + 50000);
  assert(getBuddyMood() == BuddyMood::Calm);
}

void testRolloverDecayClock() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  beginAt(anchor);
  buildTouchMood(anchor + 1, 2);
  updateBehaviorEngine(anchor + uint32_t{9999});
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(anchor + uint32_t{10000});
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(anchor + uint32_t{19999});
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(anchor + uint32_t{20000});
  assert(getBuddyMood() == BuddyMood::Calm);
}

void testEventsDoNotRestartDecayClock() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 1);  // engagement 20.
  processBuddyEvent(BuddyEvent::TouchTap, 9000);  // New streak, engagement 40.
  updateBehaviorEngine(9999);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(10000);
  assert(getBuddyMood() == BuddyMood::Calm);  // 35, no touch-clock reset.

  beginAt(0);
  buildTouchMood(1, 2);
  processBuddyEvent(BuddyEvent::SoundDetected, 9000);  // engagement 50.
  updateBehaviorEngine(20000);
  assert(getBuddyMood() == BuddyMood::Engaged);  // 40.
  updateBehaviorEngine(30000);
  assert(getBuddyMood() == BuddyMood::Calm);  // 35; sound did not reset decay.

  beginAt(0);
  buildTouchMood(1, 2);
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(9000);
  processBuddyEvent(BuddyEvent::IdleTimeout, 9001);
  updateBehaviorEngine(20000);
  assert(getBuddyMood() == BuddyMood::Calm);
}

void testDiagnosticSelectionRestartsDecayClock() {
  beginAt(0);
  setDiagnosticMood(BuddyMood::Grumpy, 1000000);
  updateBehaviorEngine(1000000);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(1009999);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(1010000);  // Canonical 0/60 decays to 0/50.
  assert(getBuddyMood() == BuddyMood::Sleepy);  // Diagnostics are not activity.

  setDiagnosticMood(BuddyMood::Sleepy, 1020000);
  updateBehaviorEngine(1029999);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  updateBehaviorEngine(1030000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void finishAt(uint32_t now) {
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(now);
}

void testInactivityBoundaryAndEligibility() {
  beginAt(0);
  updateBehaviorEngine(89999);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Calm);
  updateBehaviorEngine(90000);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Sleepy);

  beginAt(0);
  processBuddyEvent(BuddyEvent::IdleTimeout, 89990);
  updateBehaviorEngine(90000);
  expectState(BuddyCoreState::Awake, BuddyReaction::Generic, BuddyMood::Calm);
  // Completion is not activity: eligibility can change between decay ticks.
  finishAt(90001);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Sleepy);

  beginAt(0);
  soundActive = true;
  updateBehaviorEngine(90000);
  assert(getBuddyMood() == BuddyMood::Calm);
  soundActive = false;
  updateBehaviorEngine(90001);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void testSleepyAlertingAndSoundDelta() {
  beginAt(0);
  updateBehaviorEngine(90000);
  processBuddyEvent(BuddyEvent::TouchTap, 90001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 90002);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);

  beginAt(0);
  updateBehaviorEngine(90000);
  processBuddyEvent(BuddyEvent::SoundDetected, 90001);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled,
                 BuddyMood::Calm);
  // Each accepted sound adds exactly five; seven leave 35, eight reach 40.
  for (uint32_t index = 2; index <= 8; ++index) {
    processBuddyEvent(BuddyEvent::SoundDetected, 90000 + index);
    expectReaction(index == 2 ? FaceExpression::Suspicious : FaceExpression::Confused,
                   index == 2 ? ReactionSound::Suspicious : ReactionSound::Confused,
                   index < 8 ? BuddyMood::Calm : BuddyMood::Engaged);
  }
}

void testSoundSaturationWithoutIrritation() {
  beginAt(0);
  for (uint32_t now = 1; now <= 1000; ++now) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
    assert(getBuddyMood() == (now < 8 ? BuddyMood::Calm : BuddyMood::Engaged));
  }
  finishAt(1001);
  // Saturation at 100 leaves exactly 40 after twelve decay ticks, proving
  // sounds neither overflow engagement nor accumulate Grumpy irritation.
  updateBehaviorEngine(120000);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(130000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void testRealActivityRestartsInactivity() {
  const BuddyEvent events[] = {BuddyEvent::TouchTap, BuddyEvent::SoundDetected};
  for (BuddyEvent event : events) {
    beginAt(0);
    updateBehaviorEngine(89998);
    processBuddyEvent(event, 89999);
    finishAt(90000);
    assert(getBuddyMood() == BuddyMood::Calm);
    updateBehaviorEngine(179998);
    assert(getBuddyMood() == BuddyMood::Calm);
    updateBehaviorEngine(179999);
    assert(getBuddyMood() == BuddyMood::Sleepy);
  }
}

void testStrongMoodPrecedenceAndDiagnosticInactivity() {
  beginAt(0);
  // Diagnostic selection refreshes decay only, not meaningful activity. This
  // leaves strong irritation at the inactivity deadline without nine decay ticks.
  setDiagnosticMood(BuddyMood::Grumpy, 89999);
  updateBehaviorEngine(90000);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  updateBehaviorEngine(99999);
  assert(getBuddyMood() == BuddyMood::Sleepy);

  beginAt(0);
  setDiagnosticMood(BuddyMood::Engaged, 89999);
  updateBehaviorEngine(90000);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(99999);
  assert(getBuddyMood() == BuddyMood::Sleepy);

  // Real sounds can also build 100 engagement; after 90 seconds the retained
  // engagement 55 overrides Sleepy, until decay finally drops below 40.
  beginAt(0);
  for (uint32_t now = 1; now <= 20; ++now) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
  }
  finishAt(21);
  updateBehaviorEngine(90020);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(130000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void testAutonomousAndDiagnosticReactionsAreNotActivity() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::IdleTimeout, 20000);
  finishAt(20001);
  processBuddyEvent(BuddyEvent::IdleTimeout, 40000);
  finishAt(40001);
  triggerDiagnostic(DiagnosticReaction::Curious, 60000);
  finishAt(60001);
  triggerDiagnostic(DiagnosticReaction::Normal, 89999);
  updateBehaviorEngine(90000);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Sleepy);
}

void testPhysicalSleepResetsWakingInactivity() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::ButtonPressed, 10);
  constexpr uint32_t wakeAt = 8 * 60 * 60 * 1000;
  updateBehaviorEngine(wakeAt - 1);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Calm);
  // Ignored sleeping input must not build scores or wake Buddy.
  processBuddyEvent(BuddyEvent::TouchTap, wakeAt - 1);
  processBuddyEvent(BuddyEvent::SoundDetected, wakeAt - 1);
  processBuddyEvent(BuddyEvent::ButtonPressed, wakeAt);
  updateBehaviorEngine(wakeAt);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Calm);
  updateBehaviorEngine(wakeAt + 89999);
  assert(getBuddyMood() == BuddyMood::Calm);
  updateBehaviorEngine(wakeAt + 90000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void testInactivityRolloverAndReinitialization() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 45000;
  beginAt(anchor);
  updateBehaviorEngine(anchor + uint32_t{89999});
  assert(getBuddyMood() == BuddyMood::Calm);
  updateBehaviorEngine(anchor + uint32_t{90000});
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::SoundDetected, anchor + uint32_t{90001});
  expectReaction(FaceExpression::Startled, ReactionSound::Startled,
                 BuddyMood::Calm);
  finishAt(anchor + uint32_t{90002});
  updateBehaviorEngine(anchor + uint32_t{180000});
  assert(getBuddyMood() == BuddyMood::Calm);
  updateBehaviorEngine(anchor + uint32_t{180001});
  assert(getBuddyMood() == BuddyMood::Sleepy);
  beginBehaviorEngine(200000);
  updateBehaviorEngine(289999);
  assert(getBuddyMood() == BuddyMood::Calm);
  updateBehaviorEngine(290000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
}

void testContextTouchAndSoundMappings() {
  const FaceExpression touchExpressions[][4] = {
      {FaceExpression::Happy, FaceExpression::Curious, FaceExpression::Annoyed, FaceExpression::Annoyed},
      {FaceExpression::Happy, FaceExpression::Happy, FaceExpression::Curious, FaceExpression::Curious},
      {FaceExpression::Curious, FaceExpression::Annoyed, FaceExpression::Annoyed, FaceExpression::Annoyed},
      {FaceExpression::Curious, FaceExpression::Annoyed, FaceExpression::Annoyed, FaceExpression::Annoyed}};
  const ReactionSound touchSounds[][4] = {
      {ReactionSound::Happy, ReactionSound::Curious, ReactionSound::Annoyed, ReactionSound::Annoyed},
      {ReactionSound::Happy, ReactionSound::Happy, ReactionSound::Curious, ReactionSound::Curious},
      {ReactionSound::Curious, ReactionSound::Annoyed, ReactionSound::Annoyed, ReactionSound::Annoyed},
      {ReactionSound::Curious, ReactionSound::Annoyed, ReactionSound::Annoyed, ReactionSound::Annoyed}};
  const FaceExpression soundExpressions[][4] = {
      {FaceExpression::Startled, FaceExpression::Suspicious, FaceExpression::Confused, FaceExpression::Confused},
      {FaceExpression::Curious, FaceExpression::Suspicious, FaceExpression::Confused, FaceExpression::Confused},
      {FaceExpression::Suspicious, FaceExpression::Annoyed, FaceExpression::Annoyed, FaceExpression::Annoyed},
      {FaceExpression::Startled, FaceExpression::Startled, FaceExpression::Confused, FaceExpression::Confused}};
  const ReactionSound soundSounds[][4] = {
      {ReactionSound::Startled, ReactionSound::Suspicious, ReactionSound::Confused, ReactionSound::Confused},
      {ReactionSound::Curious, ReactionSound::Suspicious, ReactionSound::Confused, ReactionSound::Confused},
      {ReactionSound::Suspicious, ReactionSound::Annoyed, ReactionSound::Annoyed, ReactionSound::Annoyed},
      {ReactionSound::Startled, ReactionSound::Startled, ReactionSound::Confused, ReactionSound::Confused}};

  for (BuddyMood mood : moods) {
    for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
      beginAt();
      for (uint8_t index = 0; index < 4; ++index) {
        // Force arrival mood for each event without resetting its history.
        setDiagnosticMood(mood, 110 + index);
        processBuddyEvent(event, 110 + index);
        assert(getBuddyReaction() == BuddyReaction::Generic);
        assert(expression == touchExpressions[static_cast<uint8_t>(mood)][index]);
        assert(sound == touchSounds[static_cast<uint8_t>(mood)][index]);
        if (index == 0 && mood == BuddyMood::Sleepy) {
          assert(getBuddyMood() == BuddyMood::Calm);
        }
      }
    }

    beginAt();
    for (uint8_t index = 0; index < 4; ++index) {
      setDiagnosticMood(mood, 110 + index);
      const DiagnosticMoodState before = getDiagnosticMoodState(110 + index);
      processBuddyEvent(BuddyEvent::SoundDetected, 110 + index);
      const DiagnosticMoodState after = getDiagnosticMoodState(110 + index);
      assert(after.engagementScore == before.engagementScore + 5);
      assert(after.irritationScore == before.irritationScore);
      assert(after.inactivityMs == 0);
      assert(getBuddyReaction() == BuddyReaction::Generic);
      assert(expression == soundExpressions[static_cast<uint8_t>(mood)][index]);
      assert(sound == soundSounds[static_cast<uint8_t>(mood)][index]);
      if (mood == BuddyMood::Sleepy) {
        assert(getBuddyMood() == BuddyMood::Calm);
      }
    }
  }
  // Sleepy + first touch selects Curious although that touch makes mood Calm.
  // This distinguishes arrival mood from post-event mood without instrumentation.
}

void testFirstSoundComparisonAndThresholdCrossing() {
  const FaceExpression expressions[] = {FaceExpression::Startled,
      FaceExpression::Curious, FaceExpression::Suspicious, FaceExpression::Startled};
  const ReactionSound sounds[] = {ReactionSound::Startled,
      ReactionSound::Curious, ReactionSound::Suspicious, ReactionSound::Startled};
  for (uint8_t index = 0; index < 4; ++index) {
    beginAt();
    setDiagnosticMood(moods[index], 100);
    processBuddyEvent(BuddyEvent::SoundDetected, 110);
    const BuddyMood after = moods[index] == BuddyMood::Sleepy
        ? BuddyMood::Calm : moods[index];
    expectReaction(expressions[index], sounds[index], after);
  }

  beginAt(0);
  buildTouchMood(1, 2);  // 45 engagement.
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(20000);  // 35 engagement, Calm.
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::SoundDetected, 20001);
  // The +5 crosses into Engaged, but arrival Calm still selects Startled.
  expectReaction(FaceExpression::Startled, ReactionSound::Startled, BuddyMood::Engaged);
  assert(getDiagnosticMoodState(20001).engagementScore == 40);
}

void testSleepySecondSoundUsesArrivalMood() {
  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 110);
  setDiagnosticMood(BuddyMood::Sleepy, 111);
  processBuddyEvent(BuddyEvent::SoundDetected, 112);
  // Post-event Calm would select Suspicious at streak 2; arrival Sleepy must
  // select Startled even though the sound clears Sleepy and resets inactivity.
  expectReaction(FaceExpression::Startled, ReactionSound::Startled, BuddyMood::Calm);
  const DiagnosticMoodState state = getDiagnosticMoodState(112);
  assert(state.engagementScore == 5 && state.irritationScore == 0);
  assert(state.inactivityMs == 0);
}

void testDirectDiagnosticReactionsIgnoreMoodContext() {
  for (BuddyMood mood : moods) {
    beginAt();
    setDiagnosticMood(mood, 100);
    triggerDiagnostic(DiagnosticReaction::Happy, 110);
    expectReaction(FaceExpression::Happy, ReactionSound::Happy, mood);
    triggerDiagnostic(DiagnosticReaction::Curious, 115);
    expectReaction(FaceExpression::Curious, ReactionSound::Curious, mood);
    triggerDiagnostic(DiagnosticReaction::Annoyed, 120);
    expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed, mood);
    triggerDiagnostic(DiagnosticReaction::Startled, 125);
    expectReaction(FaceExpression::Startled, ReactionSound::Startled, mood);
  }
}

void testGrumpyTouchContextAfterPhysicalWake() {
  beginAt();
  buildTouchMood(110, 5);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, 120);
  processBuddyEvent(BuddyEvent::ButtonPressed, 130);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::TouchTap, 140);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::TouchTap, 150);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed, BuddyMood::Grumpy);
}
}  // namespace

long random(long) { return 0; }
long random(long minimum, long) { return minimum; }
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression requestedExpression) {
  expression = requestedExpression;
  faceFinished = false;
  ++faceStarts;
}
bool isFaceReactionFinished(uint32_t) { return faceFinished; }
void finishFaceReaction(uint32_t) {
  expression = FaceExpression::Normal;
  faceFinished = false;
  ++faceFinishes;
}
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}
void startReactionSound(uint32_t, ReactionSound requestedSound) {
  sound = requestedSound;
  soundActive = sound != ReactionSound::None;
  ++soundStarts;
}
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requestedSound,
                                  uint8_t requestedVariant,
                                  uint8_t &selectedVariant) {
  selectedVariant = requestedSound == ReactionSound::None
      ? DIAGNOSTIC_RANDOM_VARIANT
      : (requestedVariant == DIAGNOSTIC_RANDOM_VARIANT ? 0 : requestedVariant);
  startReactionSound(now, requestedSound);
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
  testStartupAndAwakeIdleMoods();
  testTouchCoexistenceAndCompletion();
  testTouchLadderPreservesMood();
  testSoundLadderAlertsSleepy();
  testDiagnosticPersonalitiesPreserveMood();
  testSleepWakePreservesEveryMood();
  testDiagnosticNormalPreservesMood();
  testMoodChangesPreserveHistories();
  testReinitializationResetsMood();
  testFreshTouchProgressionAndSaturation();
  testExpiredTouchWindowPreservesScores();
  testSoundAndIdleEventsPreserveAccumulatedMood();
  testSleepWakePreservesAccumulatedScores();
  testDiagnosticCanonicalScores();
  testEngagedDecayBoundariesAndRemainder();
  testGrumpyRecoveryAndMultipleTicks();
  testLongGapSaturatesAtZero();
  testSleepPausesDecayClock();
  testRolloverDecayClock();
  testEventsDoNotRestartDecayClock();
  testDiagnosticSelectionRestartsDecayClock();
  testInactivityBoundaryAndEligibility();
  testSleepyAlertingAndSoundDelta();
  testSoundSaturationWithoutIrritation();
  testRealActivityRestartsInactivity();
  testStrongMoodPrecedenceAndDiagnosticInactivity();
  testAutonomousAndDiagnosticReactionsAreNotActivity();
  testPhysicalSleepResetsWakingInactivity();
  testInactivityRolloverAndReinitialization();
  testContextTouchAndSoundMappings();
  testFirstSoundComparisonAndThresholdCrossing();
  testSleepySecondSoundUsesArrivalMood();
  testDirectDiagnosticReactionsIgnoreMoodContext();
  testGrumpyTouchContextAfterPhysicalWake();
  return 0;
}

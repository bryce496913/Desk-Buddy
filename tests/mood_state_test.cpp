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
    processBuddyEvent(BuddyEvent::Touch, 110);
    const BuddyMood moodAfterTouch =
        mood == BuddyMood::Sleepy ? BuddyMood::Calm : mood;
    expectReaction(FaceExpression::Happy, ReactionSound::Happy, moodAfterTouch);
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
                                        FaceExpression::Curious,
                                        FaceExpression::Annoyed};
  const ReactionSound sounds[] = {ReactionSound::Happy, ReactionSound::Curious,
                                  ReactionSound::Annoyed};
  for (uint8_t index = 0; index < 3; ++index) {
    processBuddyEvent(BuddyEvent::Touch, 110 + index);
    expectReaction(expressions[index], sounds[index], BuddyMood::Engaged);
  }
}

void testSoundLadderPreservesMood() {
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
    expectReaction(expressions[index], sounds[index], BuddyMood::Sleepy);
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
    processBuddyEvent(BuddyEvent::Touch, 120);
    processBuddyEvent(BuddyEvent::SoundDetected, 130);
    updateBehaviorEngine(140);
    expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, mood);
    assert(faceStarts == 0);
    processBuddyEvent(BuddyEvent::ButtonPressed, 150);
    expectState(BuddyCoreState::Awake, BuddyReaction::Idle, mood);
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
  processBuddyEvent(BuddyEvent::Touch, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::Touch, 6110);  // Original touch window boundary.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Grumpy);

  processBuddyEvent(BuddyEvent::SoundDetected, 6120);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled,
                 BuddyMood::Grumpy);
  setDiagnosticMood(BuddyMood::Sleepy, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 16120);  // Sound window boundary.
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious,
                 BuddyMood::Sleepy);
}

void testReinitializationResetsMood() {
  beginAt();
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::ButtonPressed, 110);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Grumpy);
  beginBehaviorEngine(200);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, 210);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, 220);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);
}

void testFreshTouchProgressionAndSaturation() {
  beginAt();
  const BuddyMood expectedMoods[] = {BuddyMood::Calm, BuddyMood::Engaged,
                                     BuddyMood::Engaged, BuddyMood::Engaged,
                                     BuddyMood::Grumpy};
  const FaceExpression expressions[] = {
      FaceExpression::Happy, FaceExpression::Curious, FaceExpression::Annoyed,
      FaceExpression::Annoyed, FaceExpression::Annoyed};
  const ReactionSound sounds[] = {
      ReactionSound::Happy, ReactionSound::Curious, ReactionSound::Annoyed,
      ReactionSound::Annoyed, ReactionSound::Annoyed};
  for (uint8_t index = 0; index < 5; ++index) {
    processBuddyEvent(BuddyEvent::Touch, 110 + index);
    expectReaction(expressions[index], sounds[index], expectedMoods[index]);
  }
  // Many increments would wrap either uint8_t score without saturation.
  // Both scores become high; irritation must continue to take precedence.
  for (uint32_t now = 115; now < 1115; ++now) {
    processBuddyEvent(BuddyEvent::Touch, now);
    expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed,
                   BuddyMood::Grumpy);
  }
}

void testExpiredTouchWindowPreservesScores() {
  beginAt();
  processBuddyEvent(BuddyEvent::Touch, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, 6111);
  // Two separate first touches contribute 20 + 20, reaching exactly 40.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::Touch, 12112);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);

  beginAt();
  for (uint32_t now = 110; now < 115; ++now) {
    processBuddyEvent(BuddyEvent::Touch, now);
  }
  processBuddyEvent(BuddyEvent::Touch, 6115);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Grumpy);
}

void testSoundAndIdleEventsPreserveAccumulatedMood() {
  const int touchCounts[] = {2, 5};
  for (int touchCount : touchCounts) {
    beginAt();
    for (int index = 0; index < touchCount; ++index) {
      processBuddyEvent(BuddyEvent::Touch, 110 + index);
    }
    const BuddyMood mood = touchCount == 2 ? BuddyMood::Engaged : BuddyMood::Grumpy;
    processBuddyEvent(BuddyEvent::SoundDetected, 120);
    expectReaction(FaceExpression::Startled, ReactionSound::Startled, mood);
    processBuddyEvent(BuddyEvent::SoundDetected, 121);
    expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious, mood);
    processBuddyEvent(BuddyEvent::SoundDetected, 122);
    expectReaction(FaceExpression::Confused, ReactionSound::Confused, mood);

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
    processBuddyEvent(BuddyEvent::Touch, 10000);
    expectReaction(FaceExpression::Happy, ReactionSound::Happy, mood);
  }
}

void testSleepWakePreservesAccumulatedScores() {
  // A subthreshold engagement score survives sleep and combines with a new
  // first touch after wake (20 + 20), rather than starting from zero again.
  beginAt();
  processBuddyEvent(BuddyEvent::Touch, 110);
  processBuddyEvent(BuddyEvent::ButtonPressed, 120);
  updateBehaviorEngine(1000000);
  processBuddyEvent(BuddyEvent::ButtonPressed, 1000010);
  processBuddyEvent(BuddyEvent::Touch, 1000020);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);

  // Four touches leave irritation at 50. Sleep resets the reaction streak,
  // but the third touch after wake must add 25 to that retained irritation.
  beginAt();
  for (uint32_t now = 110; now < 114; ++now) {
    processBuddyEvent(BuddyEvent::Touch, now);
  }
  processBuddyEvent(BuddyEvent::ButtonPressed, 120);
  processBuddyEvent(BuddyEvent::ButtonPressed, 130);
  processBuddyEvent(BuddyEvent::Touch, 140);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::Touch, 150);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::Touch, 160);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed,
                 BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, 170);
  expectState(BuddyCoreState::Sleeping, BuddyReaction::Idle, BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::ButtonPressed, 180);
  expectState(BuddyCoreState::Awake, BuddyReaction::Idle, BuddyMood::Grumpy);
}

void testDiagnosticCanonicalScores() {
  beginAt();
  for (uint32_t now = 110; now < 120; ++now) {
    processBuddyEvent(BuddyEvent::Touch, now);
  }
  setDiagnosticMood(BuddyMood::Calm, 100);
  processBuddyEvent(BuddyEvent::Touch, 6120);  // Fresh streak, cleared scores.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, 6121);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);

  setDiagnosticMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::Touch, 12122);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::Touch, 18123);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Grumpy);

  setDiagnosticMood(BuddyMood::Sleepy, 18123);
  updateBehaviorEngine(18124);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::SoundDetected, 18125);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::Touch, 24124);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, 24125);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious,
                 BuddyMood::Engaged);
}

void buildTouchMood(uint32_t start, int count) {
  for (int index = 0; index < count; ++index) {
    processBuddyEvent(BuddyEvent::Touch, start + index);
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
  processBuddyEvent(BuddyEvent::Touch, 50001);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Engaged);
}

void testLongGapSaturatesAtZero() {
  constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
  beginAt(0);
  updateBehaviorEngine(max);
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, max);
  assert(getBuddyMood() == BuddyMood::Calm);  // Zero + 20, no underflow.

  beginAt(0);
  buildTouchMood(1, 100);  // Both scores saturate at 100.
  updateBehaviorEngine(max);
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::Touch, max);
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
  processBuddyEvent(BuddyEvent::Touch, 1);  // engagement 20.
  processBuddyEvent(BuddyEvent::Touch, 9000);  // New streak, engagement 40.
  updateBehaviorEngine(9999);
  assert(getBuddyMood() == BuddyMood::Engaged);
  updateBehaviorEngine(10000);
  assert(getBuddyMood() == BuddyMood::Calm);  // 35, no touch-clock reset.

  beginAt(0);
  buildTouchMood(1, 2);
  processBuddyEvent(BuddyEvent::SoundDetected, 9000);
  updateBehaviorEngine(20000);
  assert(getBuddyMood() == BuddyMood::Calm);

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
  assert(getBuddyMood() == BuddyMood::Calm);

  setDiagnosticMood(BuddyMood::Sleepy, 1020000);
  updateBehaviorEngine(1029999);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  updateBehaviorEngine(1030000);
  assert(getBuddyMood() == BuddyMood::Calm);
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
  testSoundLadderPreservesMood();
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
  return 0;
}

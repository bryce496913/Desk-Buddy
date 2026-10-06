#include <cassert>
#include <cstdint>
#include <limits>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

static_assert(DESK_BUDDY_DIAGNOSTICS == 0,
              "production behavior test must compile with diagnostics off");

namespace {
FaceExpression requestedExpression = FaceExpression::Normal;
ReactionSound requestedSound = ReactionSound::None;
bool faceReactionFinished = false;
bool soundEngineActive = false;
int faceReactionStarts = 0;
int faceReactionFinishes = 0;
int soundReactionStarts = 0;
int soundReactionStops = 0;
int sleepFaceEntries = 0;
int wakeFaceRequests = 0;
int sleepSoundRequests = 0;
int wakeSoundRequests = 0;
int wakeSensorIgnores = 0;

void resetObservations() {
  requestedExpression = FaceExpression::Normal;
  requestedSound = ReactionSound::None;
  faceReactionFinished = false;
  soundEngineActive = false;
  faceReactionStarts = 0;
  faceReactionFinishes = 0;
  soundReactionStarts = 0;
  soundReactionStops = 0;
  sleepFaceEntries = 0;
  wakeFaceRequests = 0;
  sleepSoundRequests = 0;
  wakeSoundRequests = 0;
  wakeSensorIgnores = 0;
}

void beginAt(uint32_t now) {
  resetObservations();
  beginBehaviorEngine(now);
  assert(getBuddyMood() == BuddyMood::Calm);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}

void expectReaction(FaceExpression expression, ReactionSound sound) {
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(requestedExpression == expression);
  assert(requestedSound == sound);
}

void finishReactionAt(uint32_t now) {
  const BuddyMood moodBeforeCompletion = getBuddyMood();
  faceReactionFinished = true;
  soundEngineActive = false;
  updateBehaviorEngine(now);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionFinishes > 0);
  assert(getBuddyMood() == moodBeforeCompletion);
}

void testTouchWindowAndSaturation() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  assert(getBuddyMood() == BuddyMood::Calm);
  processBuddyEvent(BuddyEvent::TouchTap, 6100);  // Exactly 6000 ms is recent.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(getBuddyMood() == BuddyMood::Engaged);
  processBuddyEvent(BuddyEvent::TouchTap, 12099);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::TouchTap, 18099);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  // Streak remains saturated; pre-event Engaged selects Curious.
  processBuddyEvent(BuddyEvent::TouchTap, 18100);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  processBuddyEvent(BuddyEvent::TouchTap, 24101);  // More than 6000 ms later.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(getBuddyMood() == BuddyMood::Grumpy);
}

void testSoundWindowAndSaturation() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::SoundDetected, 100);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  processBuddyEvent(BuddyEvent::SoundDetected, 10100);  // Boundary is recent.
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  processBuddyEvent(BuddyEvent::SoundDetected, 20099);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
  processBuddyEvent(BuddyEvent::SoundDetected, 30099);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);

  processBuddyEvent(BuddyEvent::SoundDetected, 30100);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
  processBuddyEvent(BuddyEvent::SoundDetected, 40101);  // More than 10000 ms.
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
}

void testIndependentHistoriesAndReplacement() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 10);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::TouchTap, 20);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::SoundDetected, 30);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  beginAt(100);
  processBuddyEvent(BuddyEvent::SoundDetected, 110);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  processBuddyEvent(BuddyEvent::SoundDetected, 120);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  processBuddyEvent(BuddyEvent::TouchTap, 130);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  // A same-type interaction replaces an unfinished Generic reaction.
  beginAt(200);
  processBuddyEvent(BuddyEvent::TouchTap, 210);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::TouchTap, 220);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(faceReactionStarts == 2);
  assert(soundReactionStarts == 2);

  // The other interaction type can replace it as well.
  processBuddyEvent(BuddyEvent::SoundDetected, 230);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(faceReactionStarts == 3);
  assert(soundReactionStarts == 3);

  finishReactionAt(240);
  assert(requestedExpression == FaceExpression::Normal);
}

void testSleepWakeAndHistoryReset() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 10);
  processBuddyEvent(BuddyEvent::TouchTap, 20);
  processBuddyEvent(BuddyEvent::TouchTap, 30);
  processBuddyEvent(BuddyEvent::SoundDetected, 40);
  processBuddyEvent(BuddyEvent::SoundDetected, 50);
  processBuddyEvent(BuddyEvent::SoundDetected, 60);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);

  const int startsBeforeSleep = faceReactionStarts;
  processBuddyEvent(BuddyEvent::ButtonPressed, 70);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(getBuddyMood() == BuddyMood::Engaged);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(sleepFaceEntries == 1);
  assert(sleepSoundRequests == 1);
  assert(soundReactionStops >= 1);
  assert(!soundEngineActive);

  processBuddyEvent(BuddyEvent::TouchTap, 80);
  processBuddyEvent(BuddyEvent::SoundDetected, 90);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == startsBeforeSleep);

  processBuddyEvent(BuddyEvent::ButtonPressed, 100);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(wakeFaceRequests == 1);
  assert(getBuddyMood() == BuddyMood::Engaged);
  assert(wakeSoundRequests == 1);
  assert(wakeSensorIgnores == 1);
  processBuddyEvent(BuddyEvent::TouchTap, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::SoundDetected, 120);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
}

void testAutonomousPersonalityAndHistories() {
  beginAt(1000);  // The deterministic deadline is 21000.
  updateBehaviorEngine(1000);
  updateBehaviorEngine(20999);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == 0);

  updateBehaviorEngine(21000);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(requestedExpression == FaceExpression::Curious ||
         requestedExpression == FaceExpression::Daydreaming);
  assert(getBuddyMood() == BuddyMood::Calm);
  assert(requestedSound == ReactionSound::None);
  assert(!soundEngineActive);
  finishReactionAt(21500);

  // Completion schedules a new future deadline, not another immediate event.
  const int startsAfterCompletion = faceReactionStarts;
  updateBehaviorEngine(21500);
  updateBehaviorEngine(41499);
  assert(faceReactionStarts == startsAfterCompletion);
  updateBehaviorEngine(41500);
  assert(faceReactionStarts == startsAfterCompletion + 1);
  assert(getBuddyMood() == BuddyMood::Calm);
  finishReactionAt(41600);

  // Autonomous reactions do not contribute to either interaction streak.
  processBuddyEvent(BuddyEvent::TouchTap, 41610);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::SoundDetected, 41620);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
}

void testInteractionPostponesAutonomy() {
  for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
    beginAt(0);  // Initial deadline 20000.
    processBuddyEvent(event, 19999);  // New deadline 39999.
    faceReactionFinished = true;
    soundEngineActive = false;
    updateBehaviorEngine(20000);
    assert(getBuddyReaction() == BuddyReaction::Idle);
    assert(faceReactionStarts == 1);
    updateBehaviorEngine(39998);
    assert(faceReactionStarts == 1);
    updateBehaviorEngine(39999);
    assert(faceReactionStarts == 2);
  }

  beginAt(100000);  // Initial deadline 120000.
  processBuddyEvent(BuddyEvent::SoundDetected, 119999);
  faceReactionFinished = true;
  soundEngineActive = false;
  updateBehaviorEngine(120000);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == 1);
  updateBehaviorEngine(139998);
  assert(faceReactionStarts == 1);
  updateBehaviorEngine(139999);
  assert(faceReactionStarts == 2);

  beginAt(0);
  updateBehaviorEngine(20000);  // An autonomous reaction is already active.
  assert(faceReactionStarts == 1);
  processBuddyEvent(BuddyEvent::TouchHold, 20100);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  finishReactionAt(20500);
  // Hold completion must not replace its deadline with an autonomous one.
  updateBehaviorEngine(40099);
  assert(faceReactionStarts == 2);
  updateBehaviorEngine(40100);
  assert(faceReactionStarts == 3);
}

void testAutonomyDoesNotChangeDecayPolicy() {
  const int touchCounts[] = {2, 5};
  for (int touchCount : touchCounts) {
    beginAt(0);
    for (int index = 0; index < touchCount; ++index) {
      processBuddyEvent(BuddyEvent::TouchTap, 100 + index);
    }
    const BuddyMood mood = touchCount == 2 ? BuddyMood::Engaged : BuddyMood::Grumpy;
    assert(getBuddyMood() == mood);
    finishReactionAt(110);
    // Last touch schedules the deterministic autonomous deadline.
    updateBehaviorEngine(20099 + touchCount);
    assert(getBuddyReaction() == BuddyReaction::Generic);
    assert(requestedExpression == FaceExpression::Curious ||
           requestedExpression == FaceExpression::Daydreaming);
    assert(requestedSound == ReactionSound::None);
    // Two awake decay ticks precede autonomy; the reaction adds no mood effect.
    const BuddyMood decayedMood =
        touchCount == 2 ? BuddyMood::Calm : BuddyMood::Engaged;
    assert(getBuddyMood() == decayedMood);
    finishReactionAt(20110);
    assert(getBuddyMood() == decayedMood);
  }
}

void testScheduledAutonomyDoesNotPreventSleepy() {
  beginAt(0);
  // Real production scheduling runs during inactivity. Neither the reactions
  // nor their completions may postpone the 90-second meaningful-activity clock.
  const uint32_t deadlines[] = {20000, 40100, 60200, 80300};
  for (uint32_t deadline : deadlines) {
    updateBehaviorEngine(deadline);
    assert(getBuddyReaction() == BuddyReaction::Generic);
    assert(requestedExpression == FaceExpression::Curious ||
           requestedExpression == FaceExpression::Daydreaming);
    assert(requestedSound == ReactionSound::None);
    finishReactionAt(deadline + 100);
  }
  updateBehaviorEngine(89999);
  assert(getBuddyMood() == BuddyMood::Calm);
  updateBehaviorEngine(90000);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  processBuddyEvent(BuddyEvent::SoundDetected, 90001);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  assert(getBuddyMood() == BuddyMood::Calm);
}

void testSleepSuppressesAutonomy() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::ButtonPressed, 100);
  updateBehaviorEngine(1000000);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(faceReactionStarts == 0);

  processBuddyEvent(BuddyEvent::ButtonPressed, 1000000);
  updateBehaviorEngine(1000000);
  updateBehaviorEngine(1019999);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == 0);
  updateBehaviorEngine(1020000);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(requestedSound == ReactionSound::None);
}

void testRolloverSafeTiming() {
  constexpr uint32_t max = std::numeric_limits<uint32_t>::max();

  beginAt(max - 5000);
  processBuddyEvent(BuddyEvent::TouchTap, max - 3000);
  processBuddyEvent(BuddyEvent::TouchTap, 1000);  // 4001 ms elapsed.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::TouchTap, 7001);  // 6001 ms elapsed.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);

  beginAt(max - 5000);
  processBuddyEvent(BuddyEvent::SoundDetected, max - 3000);
  processBuddyEvent(BuddyEvent::SoundDetected, 6000);  // 9001 ms elapsed.
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  processBuddyEvent(BuddyEvent::SoundDetected, 16001);  // 10001 ms elapsed.
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);

  beginAt(max - 10000);  // Deadline wraps to 9999.
  updateBehaviorEngine(9998);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  updateBehaviorEngine(9999);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(requestedSound == ReactionSound::None);
}
}  // namespace

long random(long maximum) {
  assert(maximum > 0);
  return 0;  // Select Curious; production alternation may select Daydreaming.
}

long random(long minimum, long maximum) {
  assert(minimum < maximum);
  return minimum;  // Make every autonomous delay exactly 20000 ms.
}

void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression expression) {
  requestedExpression = expression;
  faceReactionFinished = false;
  faceReactionStarts++;
}
bool isFaceReactionFinished(uint32_t) { return faceReactionFinished; }
void finishFaceReaction(uint32_t) {
  requestedExpression = FaceExpression::Normal;
  faceReactionFinished = false;
  faceReactionFinishes++;
}
void enterSleepFace(uint32_t) { sleepFaceEntries++; }
void wakeFace(uint32_t) { wakeFaceRequests++; }

void startReactionSound(uint32_t, ReactionSound sound) {
  requestedSound = sound;
  soundEngineActive = sound != ReactionSound::None;
  soundReactionStarts++;
}
void stopReactionSound() {
  requestedSound = ReactionSound::None;
  soundEngineActive = false;
  soundReactionStops++;
}
bool isSoundEngineActive() { return soundEngineActive; }
void playSleepSound() { sleepSoundRequests++; }
void playWakeSound() { wakeSoundRequests++; }
void ignoreSoundSensorAfterWake() { wakeSensorIgnores++; }

int main() {
  testTouchWindowAndSaturation();
  testSoundWindowAndSaturation();
  testIndependentHistoriesAndReplacement();
  testSleepWakeAndHistoryReset();
  testAutonomousPersonalityAndHistories();
  testInteractionPostponesAutonomy();
  testAutonomyDoesNotChangeDecayPolicy();
  testScheduledAutonomyDoesNotPreventSleepy();
  testSleepSuppressesAutonomy();
  testRolloverSafeTiming();
  return 0;
}

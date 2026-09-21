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
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}

void expectReaction(FaceExpression expression, ReactionSound sound) {
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(requestedExpression == expression);
  assert(requestedSound == sound);
}

void finishReactionAt(uint32_t now) {
  faceReactionFinished = true;
  soundEngineActive = false;
  updateBehaviorEngine(now);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionFinishes > 0);
}

void testTouchWindowAndSaturation() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::Touch, 100);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::Touch, 6100);  // Exactly 6000 ms is recent.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::Touch, 12099);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed);
  processBuddyEvent(BuddyEvent::Touch, 18099);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed);

  // Further recent touches stay at the saturated Annoyed level.
  processBuddyEvent(BuddyEvent::Touch, 18100);
  expectReaction(FaceExpression::Annoyed, ReactionSound::Annoyed);
  processBuddyEvent(BuddyEvent::Touch, 24101);  // More than 6000 ms later.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
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
  processBuddyEvent(BuddyEvent::Touch, 10);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::Touch, 20);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::SoundDetected, 30);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);

  beginAt(100);
  processBuddyEvent(BuddyEvent::SoundDetected, 110);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  processBuddyEvent(BuddyEvent::SoundDetected, 120);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  processBuddyEvent(BuddyEvent::Touch, 130);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);

  // A same-type interaction replaces an unfinished Generic reaction.
  beginAt(200);
  processBuddyEvent(BuddyEvent::Touch, 210);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::Touch, 220);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  assert(faceReactionStarts == 2);
  assert(soundReactionStarts == 2);

  // The other interaction type can replace it as well.
  processBuddyEvent(BuddyEvent::SoundDetected, 230);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  assert(faceReactionStarts == 3);
  assert(soundReactionStarts == 3);

  finishReactionAt(240);
  assert(requestedExpression == FaceExpression::Normal);
}

void testSleepWakeAndHistoryReset() {
  beginAt(0);
  processBuddyEvent(BuddyEvent::Touch, 10);
  processBuddyEvent(BuddyEvent::Touch, 20);
  processBuddyEvent(BuddyEvent::Touch, 30);
  processBuddyEvent(BuddyEvent::SoundDetected, 40);
  processBuddyEvent(BuddyEvent::SoundDetected, 50);
  processBuddyEvent(BuddyEvent::SoundDetected, 60);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);

  const int startsBeforeSleep = faceReactionStarts;
  processBuddyEvent(BuddyEvent::ButtonPressed, 70);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(sleepFaceEntries == 1);
  assert(sleepSoundRequests == 1);
  assert(soundReactionStops >= 1);
  assert(!soundEngineActive);

  processBuddyEvent(BuddyEvent::Touch, 80);
  processBuddyEvent(BuddyEvent::SoundDetected, 90);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == startsBeforeSleep);

  processBuddyEvent(BuddyEvent::ButtonPressed, 100);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(wakeFaceRequests == 1);
  assert(wakeSoundRequests == 1);
  assert(wakeSensorIgnores == 1);
  processBuddyEvent(BuddyEvent::Touch, 110);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::SoundDetected, 120);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
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
  finishReactionAt(41600);

  // Autonomous reactions do not contribute to either interaction streak.
  processBuddyEvent(BuddyEvent::Touch, 41610);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::SoundDetected, 41620);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
}

void testInteractionPostponesAutonomy() {
  beginAt(0);  // Initial deadline 20000.
  processBuddyEvent(BuddyEvent::Touch, 19999);  // New deadline 39999.
  faceReactionFinished = true;
  soundEngineActive = false;
  updateBehaviorEngine(20000);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(faceReactionStarts == 1);
  updateBehaviorEngine(39998);
  assert(faceReactionStarts == 1);
  updateBehaviorEngine(39999);
  assert(faceReactionStarts == 2);

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
  processBuddyEvent(BuddyEvent::Touch, max - 3000);
  processBuddyEvent(BuddyEvent::Touch, 1000);  // 4001 ms elapsed.
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::Touch, 7001);  // 6001 ms elapsed.
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
  testSleepSuppressesAutonomy();
  testRolloverSafeTiming();
  return 0;
}

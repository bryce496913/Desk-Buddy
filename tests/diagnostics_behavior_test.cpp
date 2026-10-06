#include <cassert>
#include <cstdint>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

namespace {
FaceExpression lastExpression = FaceExpression::Normal;
ReactionSound lastSound = ReactionSound::None;
int faceReactionStarts = 0;
int soundReactionStarts = 0;
int soundReactionStops = 0;
int sleepFaceEntries = 0;
int wakeFaceRequests = 0;
int sleepSoundRequests = 0;
int wakeSoundRequests = 0;
bool faceFinished = false;
bool soundActive = false;
uint8_t lastRequestedVariant = DIAGNOSTIC_RANDOM_VARIANT;
}  // namespace

long random(long maximum) {
  (void)maximum;
  return 0;
}
long random(long minimum, long maximum) {
  (void)maximum;
  return minimum;
}

void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression expression) {
  lastExpression = expression;
  faceReactionStarts++;
  faceFinished = false;
}
bool isFaceReactionFinished(uint32_t) { return faceFinished; }
void finishFaceReaction(uint32_t) { lastExpression = FaceExpression::Normal; }
void enterSleepFace(uint32_t) { sleepFaceEntries++; }
void wakeFace(uint32_t) { wakeFaceRequests++; }

void startReactionSound(uint32_t, ReactionSound sound) {
  lastSound = sound;
  soundReactionStarts++;
  soundActive = sound != ReactionSound::None;
}
bool startDiagnosticReactionSound(uint32_t, ReactionSound sound,
                                  uint8_t requestedVariantIndex,
                                  uint8_t &selectedVariantIndex) {
  lastRequestedVariant = requestedVariantIndex;
  if (sound == ReactionSound::None) {
    selectedVariantIndex = DIAGNOSTIC_RANDOM_VARIANT;
    startReactionSound(0, sound);
    return true;
  }
  selectedVariantIndex = requestedVariantIndex == DIAGNOSTIC_RANDOM_VARIANT
      ? 2
      : requestedVariantIndex;
  startReactionSound(0, sound);
  return selectedVariantIndex < 3;
}
void stopReactionSound() {
  soundReactionStops++;
  lastSound = ReactionSound::None;
  soundActive = false;
}
bool isSoundEngineActive() { return soundActive; }
void playSleepSound() { sleepSoundRequests++; }
void playWakeSound() { wakeSoundRequests++; }
void ignoreSoundSensorAfterWake() {}

int main() {
  beginBehaviorEngine(100);
  assert(getBuddyMood() == BuddyMood::Calm);
  const BuddyMood moods[] = {BuddyMood::Engaged, BuddyMood::Grumpy,
                            BuddyMood::Sleepy, BuddyMood::Calm};
  for (BuddyMood mood : moods) {
    setDiagnosticMood(mood, 100);
    assert(getBuddyMood() == mood);
    assert(getBuddyCoreState() == BuddyCoreState::Awake);
    assert(getBuddyReaction() == BuddyReaction::Idle);
    assert(faceReactionStarts == 0);
    assert(soundReactionStarts == 0);
    assert(soundReactionStops == 0);
    assert(sleepFaceEntries == 0 && wakeFaceRequests == 0);
    assert(sleepSoundRequests == 0 && wakeSoundRequests == 0);
  }
  uint8_t selectedVariant = DIAGNOSTIC_RANDOM_VARIANT;

  const DiagnosticReaction reactions[] = {
      DiagnosticReaction::Happy,      DiagnosticReaction::Curious,
      DiagnosticReaction::Annoyed,    DiagnosticReaction::Startled,
      DiagnosticReaction::Suspicious, DiagnosticReaction::Confused,
      DiagnosticReaction::Daydreaming};
  const FaceExpression expressions[] = {
      FaceExpression::Happy,      FaceExpression::Curious,
      FaceExpression::Annoyed,    FaceExpression::Startled,
      FaceExpression::Suspicious, FaceExpression::Confused,
      FaceExpression::Daydreaming};
  const ReactionSound sounds[] = {
      ReactionSound::Happy,      ReactionSound::Curious,
      ReactionSound::Annoyed,    ReactionSound::Startled,
      ReactionSound::Suspicious, ReactionSound::Confused,
      ReactionSound::None};

  for (uint8_t index = 0; index < 7; index++) {
    assert(triggerDiagnosticReaction(reactions[index],
                                     DiagnosticSoundVariant::Random,
                                     200 + index, selectedVariant));
    assert(getBuddyReaction() == BuddyReaction::Generic);
    assert(lastExpression == expressions[index]);
    assert(lastSound == sounds[index]);
    if (sounds[index] == ReactionSound::None) {
      assert(selectedVariant == DIAGNOSTIC_RANDOM_VARIANT);
    } else {
      assert(selectedVariant == 2);
    }
  }

  assert(triggerDiagnosticReaction(DiagnosticReaction::Normal,
                                   DiagnosticSoundVariant::Variant2, 300,
                                   selectedVariant));
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(lastExpression == FaceExpression::Normal);
  assert(lastSound == ReactionSound::None);

  // A diagnostic trigger does not alter the real touch streak.
  assert(triggerDiagnosticReaction(DiagnosticReaction::Annoyed,
                                   DiagnosticSoundVariant::Variant2, 400,
                                   selectedVariant));
  assert(lastRequestedVariant == 1);
  assert(selectedVariant == 1);
  setDiagnosticMood(BuddyMood::Engaged, 100);
  const int faceStartsBeforeMood = faceReactionStarts;
  const int soundStartsBeforeMood = soundReactionStarts;
  const int soundStopsBeforeMood = soundReactionStops;
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(lastExpression == FaceExpression::Annoyed);
  assert(lastSound == ReactionSound::Annoyed);
  assert(soundActive);
  assert(faceReactionStarts == faceStartsBeforeMood);
  assert(soundReactionStarts == soundStartsBeforeMood);
  assert(soundReactionStops == soundStopsBeforeMood);
  setDiagnosticMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 401);
  assert(lastExpression == FaceExpression::Happy);
  setDiagnosticMood(BuddyMood::Sleepy, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 402);
  assert(lastExpression == FaceExpression::Annoyed);
  setDiagnosticMood(BuddyMood::Calm, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 403);
  assert(lastExpression == FaceExpression::Annoyed);

  // Nor does it alter the independent real sound streak.
  assert(triggerDiagnosticReaction(DiagnosticReaction::Confused,
                                   DiagnosticSoundVariant::Variant3, 500,
                                   selectedVariant));
  assert(lastRequestedVariant == 2);
  assert(selectedVariant == 2);
  processBuddyEvent(BuddyEvent::SoundDetected, 501);
  assert(lastExpression == FaceExpression::Confused);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 502);
  assert(lastExpression == FaceExpression::Annoyed);
  setDiagnosticMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 503);
  assert(lastExpression == FaceExpression::Confused);

  // Autonomous personalities never start in a diagnostic build.
  const int startsBeforeIdleUpdate = faceReactionStarts;
  assert(triggerDiagnosticReaction(DiagnosticReaction::Normal,
                                   DiagnosticSoundVariant::Variant1, 600,
                                   selectedVariant));
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  updateBehaviorEngine(650);
  assert(faceReactionStarts == startsBeforeIdleUpdate);

  processBuddyEvent(BuddyEvent::ButtonPressed, 700);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  assert(!triggerDiagnosticReaction(DiagnosticReaction::Happy,
                                    DiagnosticSoundVariant::Variant1, 701,
                                    selectedVariant));
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);

  const int facesBeforeSleepingMood = faceReactionStarts;
  const int soundsBeforeSleepingMood = soundReactionStarts;
  const int stopsBeforeSleepingMood = soundReactionStops;
  setDiagnosticMood(BuddyMood::Sleepy, 100);
  assert(getBuddyMood() == BuddyMood::Sleepy);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  setDiagnosticMood(BuddyMood::Grumpy, 100);
  assert(faceReactionStarts == facesBeforeSleepingMood);
  assert(soundReactionStarts == soundsBeforeSleepingMood);
  assert(soundReactionStops == stopsBeforeSleepingMood);
  assert(sleepFaceEntries == 1 && wakeFaceRequests == 0);
  assert(sleepSoundRequests == 1 && wakeSoundRequests == 0);
  processBuddyEvent(BuddyEvent::ButtonPressed, 702);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(getBuddyMood() == BuddyMood::Grumpy);
  beginBehaviorEngine(800);
  assert(getBuddyMood() == BuddyMood::Calm);

  assert(soundReactionStarts >= 1);
  return 0;
}

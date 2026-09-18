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
bool faceFinished = false;
bool soundActive = false;
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
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}

void startReactionSound(uint32_t, ReactionSound sound) {
  lastSound = sound;
  soundReactionStarts++;
  soundActive = sound != ReactionSound::None;
}
void stopReactionSound() {
  lastSound = ReactionSound::None;
  soundActive = false;
}
bool isSoundEngineActive() { return soundActive; }
void playSleepSound() {}
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}

int main() {
  beginBehaviorEngine(100);

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
    assert(triggerDiagnosticReaction(reactions[index], 200 + index));
    assert(getBuddyReaction() == BuddyReaction::Generic);
    assert(lastExpression == expressions[index]);
    assert(lastSound == sounds[index]);
  }

  assert(triggerDiagnosticReaction(DiagnosticReaction::Normal, 300));
  assert(getBuddyReaction() == BuddyReaction::Idle);
  assert(lastExpression == FaceExpression::Normal);
  assert(lastSound == ReactionSound::None);

  // A diagnostic trigger does not alter the real touch streak.
  assert(triggerDiagnosticReaction(DiagnosticReaction::Annoyed, 400));
  processBuddyEvent(BuddyEvent::Touch, 401);
  assert(lastExpression == FaceExpression::Happy);
  processBuddyEvent(BuddyEvent::Touch, 402);
  assert(lastExpression == FaceExpression::Curious);

  // Nor does it alter the independent real sound streak.
  assert(triggerDiagnosticReaction(DiagnosticReaction::Confused, 500));
  processBuddyEvent(BuddyEvent::SoundDetected, 501);
  assert(lastExpression == FaceExpression::Startled);
  processBuddyEvent(BuddyEvent::SoundDetected, 502);
  assert(lastExpression == FaceExpression::Suspicious);

  // Autonomous personalities never start in a diagnostic build.
  const int startsBeforeIdleUpdate = faceReactionStarts;
  assert(triggerDiagnosticReaction(DiagnosticReaction::Normal, 600));
  updateBehaviorEngine(1000000);
  assert(faceReactionStarts == startsBeforeIdleUpdate);

  processBuddyEvent(BuddyEvent::ButtonPressed, 700);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(!triggerDiagnosticReaction(DiagnosticReaction::Happy, 701));
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);

  assert(soundReactionStarts >= 1);
  return 0;
}

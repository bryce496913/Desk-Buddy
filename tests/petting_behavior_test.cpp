#include <cassert>
#include <cstdint>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

// Stubbed collaborators: the face finishes on demand and sounds are recorded.
FaceExpression face = FaceExpression::Normal;
bool faceFinished = false;
int faceStarts = 0;
ReactionSound lastSound = ReactionSound::None;
int soundStarts = 0;
bool audioActive = false;
void pinMode(uint8_t, uint8_t) {}
uint32_t millis() { return 0; }
long random(long) { return 0; }
long random(long low, long) { return low; }
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression expression) {
  face = expression; faceFinished = false; ++faceStarts;
}
bool isFaceReactionFinished(uint32_t) { return faceFinished; }
void finishFaceReaction(uint32_t) { face = FaceExpression::Normal; }
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}
void startReactionSound(uint32_t, ReactionSound sound, BuddyMood) {
  lastSound = sound; ++soundStarts; audioActive = sound != ReactionSound::None;
}
void stopReactionSound() { audioActive = false; }
bool isSoundEngineActive() { return audioActive; }
void playSleepSound() {}
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}

void finish(uint32_t now) {
  faceFinished = true;
  audioActive = false;
  updateBehaviorEngine(now);
}
void holdAndPet(uint32_t now) {
  processBuddyEvent(BuddyEvent::TouchHold, now);
  setBuddyPetting(true, now);
  assert(face == FaceExpression::Happy && lastSound == ReactionSound::Happy);
}

int main() {
  // Short Hold: released during the Happy reaction behaves exactly as V2.
  beginBehaviorEngine(0);
  holdAndPet(1000);
  setBuddyPetting(false, 1500);
  assert(face == FaceExpression::Happy && faceStarts == 1);
  finish(2800);
  assert(getBuddyReaction() == BuddyReaction::Idle && face == FaceExpression::Normal);

  // Sustained Hold: Petted with a purr each cycle, then a ContentBlink on release.
  beginBehaviorEngine(10000);
  faceStarts = soundStarts = 0;
  holdAndPet(10000);
  finish(11800);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(face == FaceExpression::Petted && lastSound == ReactionSound::Purr);
  finish(13600);
  assert(face == FaceExpression::Petted && faceStarts == 3 && soundStarts == 3);
  // Holding steady (no change) is not a new event.
  setBuddyPetting(true, 14000);
  assert(faceStarts == 3);
  setBuddyPetting(false, 14500);
  assert(face == FaceExpression::ContentBlink && lastSound == ReactionSound::None);
  assert(getBuddyReaction() == BuddyReaction::Generic);
  finish(16300);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  // A later release/press of nothing does not restart anything.
  setBuddyPetting(false, 17000);
  assert(face == FaceExpression::Normal);

  // Long petting is affectionate: Calm becomes Engaged and irritation drops.
  beginBehaviorEngine(20000);
  holdAndPet(20000);
  uint32_t now = 20000;
  for (int cycle = 0; cycle < 3; ++cycle) finish(now += 1800);
  assert(getBuddyMood() == BuddyMood::Engaged);

  // Quiet Mode keeps the Petted face but silences the purr.
  beginBehaviorEngine(30000);
  restoreBuddySoundMode(BuddySoundMode::Quiet);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet);
  soundStarts = 0;
  processBuddyEvent(BuddyEvent::TouchHold, 30000);
  setBuddyPetting(true, 30000);
  finish(31800);
  assert(face == FaceExpression::Petted && soundStarts == 0);
  restoreBuddySoundMode(BuddySoundMode::Normal);

  // Sleeping mid-pet ends petting; releasing later starts nothing.
  beginBehaviorEngine(40000);
  holdAndPet(40000);
  finish(41800);
  assert(face == FaceExpression::Petted);
  processBuddyEvent(BuddyEvent::ButtonShortPress, 42000);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  const int startsBefore = faceStarts;
  setBuddyPetting(false, 42500);
  assert(faceStarts == startsBefore && getBuddyReaction() == BuddyReaction::Idle);

  // A reaction that replaced Petted is not overwritten by the release blink.
  beginBehaviorEngine(50000);
  holdAndPet(50000);
  finish(51800);
  processBuddyEvent(BuddyEvent::TouchTap, 52000);
  assert(face != FaceExpression::Petted);
  const FaceExpression tapFace = face;
  setBuddyPetting(false, 52100);
  assert(face == tapFace);
}

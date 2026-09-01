#include "BehaviorEngine.h"

#include "FaceRenderer.h"
#include "SoundEngine.h"
#include "SoundSensor.h"

namespace {
BuddyCoreState coreState = BuddyCoreState::Awake;
BuddyReaction activeReaction = BuddyReaction::Idle;

void startGenericReaction(uint32_t now) {
  stopReactionSound();
  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now);
  startReactionSound(now);
}

void enterSleep(uint32_t now) {
  coreState = BuddyCoreState::Sleeping;
  activeReaction = BuddyReaction::Idle;
  enterSleepFace(now);
  stopReactionSound();
  playSleepSound();
}

void wakeBuddy(uint32_t now) {
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  wakeFace(now);
  playWakeSound();
  ignoreSoundSensorAfterWake();
}
}  // namespace

void beginBehaviorEngine(uint32_t now) {
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  scheduleFaceBehavior(now);
}

void updateBehaviorEngine(uint32_t now) {
  if (activeReaction == BuddyReaction::Generic && isFaceReactionFinished(now)) {
    activeReaction = BuddyReaction::Idle;
    finishFaceReaction(now);
  }
}

void processBuddyEvent(BuddyEvent event, uint32_t now) {
  switch (event) {
    case BuddyEvent::SleepRequested:
      if (coreState == BuddyCoreState::Awake) enterSleep(now);
      break;
    case BuddyEvent::Wake:
      if (coreState == BuddyCoreState::Sleeping) wakeBuddy(now);
      break;
    case BuddyEvent::Touch:
    case BuddyEvent::SoundDetected:
      if (coreState == BuddyCoreState::Awake) startGenericReaction(now);
      break;
  }
}

BuddyCoreState getBuddyCoreState() { return coreState; }
BuddyReaction getBuddyReaction() { return activeReaction; }

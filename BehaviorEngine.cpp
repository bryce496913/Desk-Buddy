#include "BehaviorEngine.h"

#include "FaceRenderer.h"
#include "SoundEngine.h"
#include "SoundSensor.h"

namespace {
constexpr uint32_t TOUCH_REPEAT_WINDOW_MS = 6000;

BuddyCoreState coreState = BuddyCoreState::Awake;
BuddyReaction activeReaction = BuddyReaction::Idle;
uint32_t lastTouchAt = 0;
uint8_t touchStreak = 0;

void resetTouchHistory() {
  lastTouchAt = 0;
  touchStreak = 0;
}

void startGenericReaction(uint32_t now, FaceExpression expression,
                          ReactionSound sound) {
  stopReactionSound();
  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now, expression);
  startReactionSound(now, sound);
}

void enterSleep(uint32_t now) {
  coreState = BuddyCoreState::Sleeping;
  activeReaction = BuddyReaction::Idle;
  resetTouchHistory();
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
  resetTouchHistory();
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
    case BuddyEvent::ButtonPressed:
      if (coreState == BuddyCoreState::Sleeping) {
        wakeBuddy(now);
      } else {
        enterSleep(now);
      }
      break;
    case BuddyEvent::Touch:
      if (coreState == BuddyCoreState::Awake) {
        if (touchStreak == 0 ||
            static_cast<uint32_t>(now - lastTouchAt) >
                TOUCH_REPEAT_WINDOW_MS) {
          touchStreak = 1;
        } else if (touchStreak < 3) {
          touchStreak++;
        }
        lastTouchAt = now;

        if (touchStreak == 1) {
          startGenericReaction(now, FaceExpression::Happy,
                               ReactionSound::Happy);
        } else if (touchStreak == 2) {
          startGenericReaction(now, FaceExpression::Curious,
                               ReactionSound::Curious);
        } else {
          startGenericReaction(now, FaceExpression::Annoyed,
                               ReactionSound::Annoyed);
        }
      }
      break;
    case BuddyEvent::SoundDetected:
      if (coreState == BuddyCoreState::Awake)
        startGenericReaction(now, FaceExpression::Startled,
                             ReactionSound::Startled);
      break;
  }
}

BuddyCoreState getBuddyCoreState() { return coreState; }
BuddyReaction getBuddyReaction() { return activeReaction; }

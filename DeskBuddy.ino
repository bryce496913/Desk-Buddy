#include <Arduino.h>

#include "BehaviorEngine.h"
#include "Config.h"
#include "Diagnostics.h"
#include "FaceRenderer.h"
#include "Inputs.h"
#include "SoundEngine.h"
#include "SoundSensor.h"

void setup() {
  Serial.begin(115200);

  beginInputs();
  beginSoundEngine();
  beginSoundSensor();
  beginFaceRenderer();

  randomSeed(micros());
  beginBehaviorEngine(millis());

#if DESK_BUDDY_DIAGNOSTICS
  beginDiagnostics();
#endif

  Serial.println("TTP223 touch enabled on GP5 (VCC: VBUS)");
  playBootSound();
  finishSoundSensorStartup();
}

void loop() {
  uint32_t now = millis();
  BuddyCoreState coreState = getBuddyCoreState();
  bool reactionActive = getBuddyReaction() == BuddyReaction::Generic;

  if (updateSoundSensor(now, coreState == BuddyCoreState::Sleeping,
                        reactionActive, isSoundEngineActive())) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
  }

  InputEvents inputEvents = updateInputs(now);
  switch (inputEvents.touch) {
    case TouchGesture::Tap:
      processBuddyEvent(BuddyEvent::TouchTap, now);
      break;
    case TouchGesture::Hold:
      processBuddyEvent(BuddyEvent::TouchHold, now);
      break;
    case TouchGesture::None:
    default:
      break;
  }
  switch (inputEvents.button) {
    case ButtonGesture::ShortPress:
      processBuddyEvent(BuddyEvent::ButtonShortPress, now);
      break;
    case ButtonGesture::LongPress:
      processBuddyEvent(BuddyEvent::ButtonLongPress, now);
      break;
    case ButtonGesture::None:
      break;
  }

#if DESK_BUDDY_DIAGNOSTICS
  updateDiagnostics(now);
#endif

  updateBehaviorEngine(now);
  updateFaceRenderer(now, getBuddyCoreState(), getBuddyReaction());
  updateSoundEngine(now, getBuddyReaction());
}

#include <Arduino.h>

#include "BehaviorEngine.h"
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

  Serial.println("TTP223 touch enabled on GP5 (VCC: VBUS)");
  playBootSound();
  finishSoundSensorStartup();
}

void loop() {
  uint32_t now = millis();
  BuddyCoreState coreState = getBuddyCoreState();
  BuddyReaction reaction = getBuddyReaction();

  if (updateSoundSensor(now, coreState == BuddyCoreState::Sleeping,
                        reaction != BuddyReaction::Idle || isReactionAudioActive())) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
  }

  InputEvents inputEvents = updateInputs(now);
  if (inputEvents.touch) processBuddyEvent(BuddyEvent::Touch, now);
  if (inputEvents.button) processBuddyEvent(BuddyEvent::ButtonPressed, now);

  updateBehaviorEngine(now);
  updateFaceRenderer(now, getBuddyCoreState(), getBuddyReaction());
  updateSoundEngine(now, getBuddyReaction());
}

#include "SoundSensor.h"

#include <Arduino.h>
#include "Config.h"

namespace {
constexpr uint32_t SOUND_EVENT_COOLDOWN_MS = 2500;
constexpr uint32_t SOUND_STARTUP_IGNORE_MS = 250;
volatile bool soundActivationPending = false;
uint32_t soundCooldownUntil = 0;
uint32_t soundIgnoreUntil = 0;

void captureSoundActivation() { soundActivationPending = true; }
}

void beginSoundSensor() {
  pinMode(SOUND_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_SENSOR_PIN), captureSoundActivation, FALLING);
  Serial.println("VKLSVAN sound sensor ready on GP26 (VCC: 3V3)");
  Serial.println("Trigger level: ACTIVE LOW (HIGH = idle, LOW = sound)");
}

bool updateSoundSensor(uint32_t now, bool sleeping, bool reactionActive) {
  noInterrupts();
  bool activationCaptured = soundActivationPending;
  soundActivationPending = false;
  interrupts();
  if (!activationCaptured) return false;

  bool ignored = (int32_t)(now - soundIgnoreUntil) < 0;
  bool coolingDown = (int32_t)(now - soundCooldownUntil) < 0;
  if (ignored || coolingDown || reactionActive || sleeping) return false;

  Serial.println("SOUND EVENT");
  soundCooldownUntil = now + SOUND_EVENT_COOLDOWN_MS;
  return true;
}

void finishSoundSensorStartup() {
  noInterrupts();
  soundActivationPending = false;
  interrupts();
  soundIgnoreUntil = millis() + SOUND_STARTUP_IGNORE_MS;
}

void ignoreSoundSensorAfterWake() {
  soundIgnoreUntil = millis() + SOUND_STARTUP_IGNORE_MS;
}

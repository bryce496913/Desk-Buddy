#include <cassert>
#include <cstdint>

#include "Arduino.h"

namespace {
uint32_t fakeMillis = 0;
uint8_t configuredPin = UINT8_MAX;
uint8_t configuredMode = UINT8_MAX;
int registeredInterrupt = -1;
int registeredMode = -1;
void (*registeredIsr)() = nullptr;
}

HardwareSerial Serial;

void pinMode(uint8_t pin, uint8_t mode) {
  configuredPin = pin;
  configuredMode = mode;
}

int digitalPinToInterrupt(uint8_t pin) { return 100 + pin; }

void attachInterrupt(int interrupt, void (*callback)(), int mode) {
  registeredInterrupt = interrupt;
  registeredIsr = callback;
  registeredMode = mode;
}

void noInterrupts() {}
void interrupts() {}
void noTone(uint8_t) {}
void tone(uint8_t, unsigned int) {}
uint32_t millis() { return fakeMillis; }
long random(long maximum) { return maximum == 0 ? 0 : 1 % maximum; }
long random(long minimum, long) { return minimum; }

// Include the production implementation so the assertions exercise the exact
// code compiled into the firmware. It also lets each scenario reset file-local
// state without adding test hooks to the production interface.
#include "../SoundSensor.cpp"

namespace {
void resetSensor(uint32_t now = 1000) {
  fakeMillis = now;
  soundActivationPending = false;
  soundCooldownUntil = now;
  soundIgnoreUntil = now;
  buddyAudioWasActive = false;
}

void fallingEdge() {
  assert(registeredIsr != nullptr);
  registeredIsr();
}

bool update(uint32_t now, bool sleeping = false,
            bool reactionActive = false, bool buddyAudioActive = false) {
  fakeMillis = now;
  return updateSoundSensor(now, sleeping, reactionActive, buddyAudioActive);
}

void assertRejectedAndConsumed(uint32_t rejectedAt, bool sleeping = false,
                               bool reactionActive = false,
                               bool buddyAudioActive = false,
                               uint32_t eligibleAt = 0) {
  fallingEdge();
  assert(!update(rejectedAt, sleeping, reactionActive, buddyAudioActive));
  if (eligibleAt == 0) eligibleAt = rejectedAt;
  assert(!update(eligibleAt));
}

void testInterruptSetup() {
  configuredPin = UINT8_MAX;
  configuredMode = UINT8_MAX;
  registeredInterrupt = -1;
  registeredMode = -1;
  registeredIsr = nullptr;

  beginSoundSensor();

  assert(configuredPin == SOUND_SENSOR_PIN);
  assert(configuredMode == INPUT);
  assert(registeredInterrupt == digitalPinToInterrupt(SOUND_SENSOR_PIN));
  assert(registeredMode == FALLING);
  assert(registeredIsr != nullptr);
}

void testStartupIgnoreConsumesEdge() {
  resetSensor(1000);
  finishSoundSensorStartup();
  assertRejectedAndConsumed(1100, false, false, false, 1250);
  fallingEdge();
  assert(update(1250));
}

void testAcceptedEventCooldownAndBoundary() {
  resetSensor(1000);
  fallingEdge();
  assert(update(1000));
  assert(soundCooldownUntil == 3500);

  // Rejections are consumed and neither a mid-cooldown nor boundary-minus-one
  // edge is allowed to move the accepted-event deadline.
  assertRejectedAndConsumed(2000, false, false, false, 2001);
  assert(soundCooldownUntil == 3500);
  assertRejectedAndConsumed(3499, false, false, false, 3500);
  assert(soundCooldownUntil == 3500);

  fallingEdge();
  assert(update(3500));
  assert(soundCooldownUntil == 6000);
}

void testStateSuppressionsConsumeEdges() {
  resetSensor();
  assertRejectedAndConsumed(1000, true, false, false, 1001);
  fallingEdge();
  assert(update(1001));

  resetSensor();
  assertRejectedAndConsumed(1000, false, true, false, 1001);
  fallingEdge();
  assert(update(1001));

  resetSensor();
  fallingEdge();
  assert(!update(1000, false, false, true));
  // Going inactive starts the separate post-audio settling gate, but the edge
  // rejected while audio was active must already be gone.
  assert(!update(1001));
  assert(soundIgnoreUntil == 1251);
  assert(soundCooldownUntil == 1000);
  fallingEdge();
  assert(update(1251));
}

void testAudioFinishedSettlingWithoutPendingEdge() {
  resetSensor(1000);
  assert(!update(1000, false, false, true));
  assert(!update(1500, false, false, false));
  assert(soundIgnoreUntil == 1750);

  assertRejectedAndConsumed(1600, false, false, false, 1750);
  assert(soundCooldownUntil == 1000);
  fallingEdge();
  assert(update(1750));
}

void testWakeIgnoreConsumesEdge() {
  resetSensor(2000);
  ignoreSoundSensorAfterWake();
  assertRejectedAndConsumed(2249, false, false, false, 2250);
  fallingEdge();
  assert(update(2250));
}

void testCooldownRollover() {
  constexpr uint32_t acceptedAt = UINT32_MAX - 1000;
  resetSensor(acceptedAt);
  fallingEdge();
  assert(update(acceptedAt));
  assert(soundCooldownUntil == 1499);

  fallingEdge();
  assert(!update(1498));
  assert(soundCooldownUntil == 1499);
  assert(!update(1499));  // The rejected edge was consumed.
  fallingEdge();
  assert(update(1499));
}

void testStartupAndWakeIgnoreRollover() {
  constexpr uint32_t beforeWrap = UINT32_MAX - 100;
  resetSensor(beforeWrap);
  finishSoundSensorStartup();
  assert(soundIgnoreUntil == 149);
  assertRejectedAndConsumed(UINT32_MAX - 50, false, false, false, 149);
  fallingEdge();
  assert(update(149));

  resetSensor(beforeWrap);
  ignoreSoundSensorAfterWake();
  assert(soundIgnoreUntil == 149);
  assertRejectedAndConsumed(100, false, false, false, 149);
  fallingEdge();
  assert(update(149));
}

void testPostAudioIgnoreRollover() {
  constexpr uint32_t beforeWrap = UINT32_MAX - 100;
  resetSensor(beforeWrap);
  assert(!update(beforeWrap, false, false, true));
  assert(!update(UINT32_MAX - 50, false, false, false));
  assert(soundIgnoreUntil == 199);

  assertRejectedAndConsumed(100, false, false, false, 199);
  fallingEdge();
  assert(update(199));
}
}  // namespace

int main() {
  testInterruptSetup();
  testStartupIgnoreConsumesEdge();
  testAcceptedEventCooldownAndBoundary();
  testStateSuppressionsConsumeEdges();
  testAudioFinishedSettlingWithoutPendingEdge();
  testWakeIgnoreConsumesEdge();
  testCooldownRollover();
  testStartupAndWakeIgnoreRollover();
  testPostAudioIgnoreRollover();
  return 0;
}

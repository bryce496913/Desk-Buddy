#include "Inputs.h"
#include "Config.h"

#include <cassert>
#include <cstdint>
#include <limits>

namespace {
uint32_t clockNow = 0;
bool touchHigh = false;
bool buttonHigh = true;
uint8_t touchMode = 255;
uint8_t buttonMode = 255;

void beginAt(uint32_t now = 0, bool touched = false, bool buttonPressed = false) {
  clockNow = now;
  touchHigh = touched;
  buttonHigh = !buttonPressed;
  beginInputs();
  assert(touchMode == INPUT);
  assert(buttonMode == INPUT_PULLUP);
}

void expect(uint32_t now, TouchGesture gesture = TouchGesture::None,
            bool button = false) {
  clockNow = now;
  const InputEvents events = updateInputs(now);
  assert(events.touch == gesture);
  assert(events.button == button);
}

uint32_t press(uint32_t rawAt) {
  touchHigh = true;
  expect(rawAt);
  expect(rawAt + 25);  // Preserve the existing >25 ms debounce.
  expect(rawAt + 26);
  return rawAt + 26;
}

void release(uint32_t rawAt, TouchGesture gesture) {
  touchHigh = false;
  expect(rawAt);
  expect(rawAt + 25);
  expect(rawAt + 26, gesture);
  expect(rawAt + 100);
}

void testTapAndBoundary() {
  for (uint32_t duration : {150u, 690u, 699u, 700u, 800u}) {
    beginAt();
    const uint32_t pressedAt = press(100);
    expect(pressedAt + (duration > 600 ? 600 : duration - 1));
    release(pressedAt + duration,
            duration < 700 ? TouchGesture::Tap : TouchGesture::Hold);
  }
}

void testHoldWhileHighAndLongHold() {
  beginAt();
  const uint32_t pressedAt = press(100);
  expect(pressedAt + 699);
  expect(pressedAt + 700, TouchGesture::Hold);
  for (uint32_t elapsed = 701; elapsed <= 10000; elapsed += 17) {
    expect(pressedAt + elapsed);
  }
  release(pressedAt + 10001, TouchGesture::None);
  const uint32_t nextPress = press(11000);
  release(nextPress + 150, TouchGesture::Tap);
}

void testBounce() {
  beginAt();
  // Idle HIGH glitch never reaches a stable press.
  touchHigh = true;
  expect(10);
  expect(30);
  touchHigh = false;
  expect(31);
  expect(100);

  touchHigh = true;
  expect(200);
  touchHigh = false;
  expect(210);
  touchHigh = true;
  expect(220);
  expect(245);
  expect(246);  // Timer starts here, after the final noisy edge.
  expect(920);  // No Hold based on the first raw edge at 200.
  touchHigh = false;
  expect(936);  // LOW candidate at +690.
  expect(951);  // Threshold passed, but release is still pending.
  touchHigh = true;
  expect(952, TouchGesture::Hold);  // Bounce did not reset timer.
  touchHigh = false;
  expect(960);
  touchHigh = true;
  expect(970);  // No second Hold on another bounce.
  release(1000, TouchGesture::None);

  beginAt();
  const uint32_t pressedAt = press(100);
  touchHigh = false;
  expect(pressedAt + 50);
  touchHigh = true;
  expect(pressedAt + 60);
  release(pressedAt + 150, TouchGesture::Tap);
}

void testBootHighAndReinitialization() {
  beginAt(100, true);
  expect(1000);
  release(1100, TouchGesture::None);
  const uint32_t pressedAt = press(1300);
  release(pressedAt + 100, TouchGesture::Tap);
  press(1600);
  beginAt(1700, true);  // Also clear an in-progress gesture on reinitialization.
  expect(3000);
  release(3100, TouchGesture::None);
}

void testRollover() {
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  for (uint32_t duration : {150u, 690u, 699u, 700u, 800u}) {
    beginAt(maximum - 400);
    const uint32_t pressedAt = press(maximum - 300);
    release(pressedAt + duration,
            duration < 700 ? TouchGesture::Tap : TouchGesture::Hold);
  }
  beginAt(maximum - 400);
  const uint32_t pressedAt = press(maximum - 300);
  expect(pressedAt + 699);
  expect(pressedAt + 700, TouchGesture::Hold);
  release(pressedAt + 900, TouchGesture::None);

  // Debounced press itself crosses rollover.
  beginAt(maximum - 20);
  const uint32_t crossedPress = press(maximum - 10);
  release(crossedPress + 699, TouchGesture::Tap);
}

void testButton() {
  beginAt();
  buttonHigh = false;
  expect(10);
  buttonHigh = true;
  expect(20);  // Short active-LOW glitch.
  expect(100);
  buttonHigh = false;
  expect(200);
  expect(225);
  expect(226, TouchGesture::None, true);
  expect(1000);  // Held button does not repeat.
  buttonHigh = true;
  expect(1100);
  buttonHigh = false;
  expect(1110);  // Release bounce does not create another press.
  expect(1200);
  buttonHigh = true;
  expect(1300);
  expect(1326);
  buttonHigh = false;
  expect(1400);
  expect(1426, TouchGesture::None, true);

  beginAt(0, false, true);
  expect(1000);  // Boot LOW does not synthesize a button event.
  buttonHigh = true;
  expect(1100);
  expect(1126);
  buttonHigh = false;
  expect(1200);
  expect(1226, TouchGesture::None, true);

  // Both inputs can generate their independent events in the same update.
  beginAt();
  const uint32_t pressedAt = press(100);
  buttonHigh = false;
  expect(pressedAt + 674);
  expect(pressedAt + 700, TouchGesture::Hold, true);

  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  beginAt(maximum - 20);
  buttonHigh = false;
  expect(maximum - 10);
  expect(14);
  expect(15, TouchGesture::None, true);
}
}  // namespace

uint32_t millis() { return clockNow; }
int digitalRead(uint8_t pin) {
  assert(pin == TOUCH_PIN || pin == BUTTON_PIN);
  return (pin == TOUCH_PIN ? touchHigh : buttonHigh) ? HIGH : LOW;
}
void pinMode(uint8_t pin, uint8_t mode) {
  assert(pin == TOUCH_PIN || pin == BUTTON_PIN);
  (pin == TOUCH_PIN ? touchMode : buttonMode) = mode;
}

int main() {
  testTapAndBoundary();
  testHoldWhileHighAndLongHold();
  testBounce();
  testBootHighAndReinitialization();
  testRollover();
  testButton();
}

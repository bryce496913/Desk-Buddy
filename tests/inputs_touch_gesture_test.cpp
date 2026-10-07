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
            ButtonGesture button = ButtonGesture::None) {
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

uint32_t pressButton(uint32_t rawAt) {
  buttonHigh = false;
  expect(rawAt);
  expect(rawAt + 25);
  expect(rawAt + 26);
  return rawAt + 26;
}
void releaseButton(uint32_t rawAt, ButtonGesture gesture) {
  buttonHigh = true;
  expect(rawAt);
  expect(rawAt + 25);
  expect(rawAt + 26, TouchGesture::None, gesture);
  expect(rawAt + 100);
}
void testButton() {
  for (uint32_t anchor : {0u, std::numeric_limits<uint32_t>::max() - 400}) {
    for (uint32_t duration : {200u, 990u, 999u, 1000u, 1100u}) {
      beginAt(anchor);
      const uint32_t pressedAt = pressButton(anchor + 100);
      expect(pressedAt + 900 * (duration > 900));
      // First release observation may itself be after the threshold (sparse loop).
      releaseButton(pressedAt + duration, duration < 1000
          ? ButtonGesture::ShortPress : ButtonGesture::LongPress);
    }
    beginAt(anchor);
    const uint32_t pressedAt = pressButton(anchor + 100);
    expect(pressedAt + 999);
    expect(pressedAt + 1000, TouchGesture::None, ButtonGesture::LongPress);
    for (uint32_t elapsed = 1001; elapsed <= 5000; elapsed += 17)
      expect(pressedAt + elapsed);
    releaseButton(pressedAt + 5001, ButtonGesture::None);
    const uint32_t freshPress = pressButton(pressedAt + 6000);
    releaseButton(freshPress + 100, ButtonGesture::ShortPress);

    beginAt(anchor);
    const uint32_t sparsePress = pressButton(anchor + 100);
    expect(sparsePress + 900);
    expect(sparsePress + 1100, TouchGesture::None, ButtonGesture::LongPress);
    releaseButton(sparsePress + 1200, ButtonGesture::None);
  }
  // Raw glitches and noisy press edges never synthesize gestures.
  beginAt(); buttonHigh = false; expect(10);
  buttonHigh = true; expect(20); expect(100);
  buttonHigh = false; expect(200);
  buttonHigh = true; expect(210);
  buttonHigh = false; expect(220); expect(245); expect(246);
  expect(1220); // Not long based on the first raw edge at 200.
  buttonHigh = true; expect(1236); expect(1251);
  buttonHigh = false; expect(1252, TouchGesture::None, ButtonGesture::LongPress);
  buttonHigh = true; expect(1260);
  buttonHigh = false; expect(1270); // Further release bounce cannot emit again.
  releaseButton(1300, ButtonGesture::None);
  beginAt(); const uint32_t shortPress = pressButton(100);
  buttonHigh = true; expect(shortPress + 50);
  buttonHigh = false; expect(shortPress + 60);
  releaseButton(shortPress + 200, ButtonGesture::ShortPress);

  beginAt(0, false, true); expect(5000);
  releaseButton(5100, ButtonGesture::None);
  const uint32_t newPress = pressButton(5300);
  releaseButton(newPress + 999, ButtonGesture::ShortPress);
  pressButton(6500);
  beginAt(6600, false, true); expect(9000); // Reinitialization also discards an active gesture.
  releaseButton(9100, ButtonGesture::None);

  // Both classifiers can emit in one update without sharing state or thresholds.
  beginAt();
  const uint32_t touchPress = press(100);
  const uint32_t buttonPress = pressButton(200);
  buttonHigh = true; expect(touchPress + 674);
  expect(touchPress + 700, TouchGesture::Hold, ButtonGesture::ShortPress);
  release(touchPress + 800, TouchGesture::None);
  assert(buttonPress == 226);
  beginAt();
  const uint32_t longButton = pressButton(100);
  const uint32_t laterTouch = press(400);
  expect(longButton + 1000, TouchGesture::Hold, ButtonGesture::LongPress);
  assert(laterTouch == 426);
  releaseButton(longButton + 1100, ButtonGesture::None);
  release(longButton + 1200, TouchGesture::None);

  // Press debounce itself crosses UINT32_MAX.
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  beginAt(maximum - 20);
  const uint32_t wrappedPress = pressButton(maximum - 10);
  releaseButton(wrappedPress + 999, ButtonGesture::ShortPress);
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

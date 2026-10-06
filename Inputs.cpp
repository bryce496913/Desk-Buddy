#include "Inputs.h"

#include <Arduino.h>
#include "Config.h"

namespace {
constexpr uint32_t TOUCH_HOLD_MS = 700;
bool touchLastRaw = false;
bool touchStable = false;
uint32_t touchDebounceAt = 0;
uint32_t touchPressedAt = 0;
bool touchGestureActive = false;
bool touchHoldEmitted = false;
bool buttonLastRaw = false;
bool buttonStable = false;
uint32_t buttonDebounceAt = 0;
}

void beginInputs() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Seed the debouncer from the pins instead of assuming their startup state.
  // The button is active LOW because INPUT_PULLUP is used.
  touchLastRaw = touchStable = digitalRead(TOUCH_PIN) == HIGH;
  buttonLastRaw = buttonStable = digitalRead(BUTTON_PIN) == LOW;
  uint32_t now = millis();
  touchDebounceAt = now;
  buttonDebounceAt = now;
  // A press already present at boot is not a new gesture.
  touchGestureActive = false;
  touchHoldEmitted = false;
  touchPressedAt = now;
}

InputEvents updateInputs(uint32_t now) {
  InputEvents events = {TouchGesture::None, false};
  bool rawTouch = digitalRead(TOUCH_PIN) == HIGH;
  if (rawTouch != touchLastRaw) {
    touchLastRaw = rawTouch;
    touchDebounceAt = now;
  }
  if ((now - touchDebounceAt) > 25 && rawTouch != touchStable) {
    touchStable = rawTouch;
    if (touchStable) {
      touchPressedAt = now;
      touchGestureActive = true;
      touchHoldEmitted = false;
    } else if (touchGestureActive) {
      if (!touchHoldEmitted) {
        // Use the start of this stable LOW candidate, not its confirmation.
        // This also recognizes a Hold missed by sparse updates while HIGH.
        events.touch = static_cast<uint32_t>(touchDebounceAt - touchPressedAt) >=
                               TOUCH_HOLD_MS
            ? TouchGesture::Hold : TouchGesture::Tap;
      }
      touchGestureActive = false;
    }
  }

  // Wait out a pending release: its debounce must not lengthen the gesture.
  // A short LOW bounce leaves the press timer intact when HIGH returns.
  if (touchGestureActive && rawTouch && !touchHoldEmitted &&
      static_cast<uint32_t>(now - touchPressedAt) >= TOUCH_HOLD_MS) {
    events.touch = TouchGesture::Hold;
    touchHoldEmitted = true;
  }

  bool rawButton = digitalRead(BUTTON_PIN) == LOW;
  if (rawButton != buttonLastRaw) {
    buttonLastRaw = rawButton;
    buttonDebounceAt = now;
  }
  if ((now - buttonDebounceAt) > 25 && rawButton != buttonStable) {
    buttonStable = rawButton;
    if (buttonStable) {
      events.button = true;
    }
  }
  return events;
}

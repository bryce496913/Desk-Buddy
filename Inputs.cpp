#include "Inputs.h"

#include <Arduino.h>
#include "Config.h"

namespace {
bool touchLastRaw = false;
bool touchStable = false;
uint32_t touchDebounceAt = 0;
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
}

InputEvents updateInputs(uint32_t now) {
  InputEvents events = {false, false};
  bool rawTouch = digitalRead(TOUCH_PIN) == HIGH;
  if (rawTouch != touchLastRaw) {
    touchLastRaw = rawTouch;
    touchDebounceAt = now;
  }
  if ((now - touchDebounceAt) > 25 && rawTouch != touchStable) {
    touchStable = rawTouch;
    if (touchStable) {
      events.touch = true;
    }
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

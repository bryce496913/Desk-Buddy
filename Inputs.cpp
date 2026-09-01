#include "Inputs.h"

#include <Arduino.h>
#include "Config.h"

namespace {
bool touchLastRaw = false;
bool touchStable = false;
uint32_t touchDebounceAt = 0;
bool buttonLastRaw = true;
bool buttonStable = true;
uint32_t buttonDebounceAt = 0;
}

void beginInputs() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

InputEvents updateInputs(uint32_t now, BuddyCoreState coreState) {
  InputEvents events = {false, false, BuddyEvent::SleepRequested};
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

  bool rawButton = digitalRead(BUTTON_PIN) == HIGH;
  if (rawButton != buttonLastRaw) {
    buttonLastRaw = rawButton;
    buttonDebounceAt = now;
  }
  if ((now - buttonDebounceAt) > 25 && rawButton != buttonStable) {
    buttonStable = rawButton;
    if (buttonStable == LOW) {
      events.button = true;
      events.buttonEvent = coreState == BuddyCoreState::Sleeping
          ? BuddyEvent::Wake : BuddyEvent::SleepRequested;
    }
  }
  return events;
}

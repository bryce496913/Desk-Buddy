#include "SoundEngine.h"

#include <Arduino.h>
#include "Config.h"

namespace {
const uint16_t reactNotes[] = {988, 1319, 1175, 1760, 1480, 1047, 1568};
const uint16_t reactDurs[] = {65, 55, 55, 75, 65, 55, 85};
const uint8_t REACT_COUNT = sizeof(reactNotes) / sizeof(reactNotes[0]);
uint32_t nextReactNoteAt = 0;
uint32_t reactNoteEndsAt = 0;
uint8_t reactStep = 0;
bool reactNotePlaying = false;

void playSequenceBlocking(const uint16_t *notes, const uint16_t *durs,
                          uint8_t count, uint16_t gapMs) {
  for (uint8_t i = 0; i < count; i++) {
    tone(BUZZER_PIN, notes[i]);
    delay(durs[i]);
    noTone(BUZZER_PIN);
    delay(gapMs);
  }
  noTone(BUZZER_PIN);
}
}

void beginSoundEngine() { pinMode(BUZZER_PIN, OUTPUT); }

void stopReactionSound() {
  noTone(BUZZER_PIN);
  reactNotePlaying = false;
}

void playBootSound() {
  const uint16_t notes[] = {880, 1320, 1760};
  const uint16_t durs[] = {60, 60, 90};
  playSequenceBlocking(notes, durs, 3, 20);
}

void playSleepSound() {
  const uint16_t notes[] = {700, 560, 420};
  const uint16_t durs[] = {80, 90, 110};
  playSequenceBlocking(notes, durs, 3, 24);
}

void playWakeSound() {
  const uint16_t notes[] = {660, 980, 1480};
  const uint16_t durs[] = {60, 60, 90};
  playSequenceBlocking(notes, durs, 3, 20);
}

void startReactionSound(uint32_t now) {
  reactStep = 0;
  nextReactNoteAt = now;
}

void updateSoundEngine(uint32_t now, BuddyReaction reaction) {
  if (reaction != BuddyReaction::Generic) {
    if (reactNotePlaying) stopReactionSound();
    return;
  }
  if (reactNotePlaying) {
    if ((int32_t)(now - reactNoteEndsAt) < 0) return;
    noTone(BUZZER_PIN);
    reactNotePlaying = false;
    nextReactNoteAt = now + 18;
    return;
  }
  if (reactStep < REACT_COUNT && (int32_t)(now - nextReactNoteAt) >= 0) {
    tone(BUZZER_PIN, reactNotes[reactStep]);
    reactNoteEndsAt = now + reactDurs[reactStep];
    reactNotePlaying = true;
    reactStep++;
  }
}

bool isReactionAudioActive() { return reactNotePlaying; }

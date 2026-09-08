#include "SoundEngine.h"

#include <Arduino.h>
#include "Config.h"

namespace {
enum class SoundSequence : uint8_t { None, Boot, Reaction, Sleep, Wake };

struct SequenceDefinition {
  const uint16_t *notes;
  const uint16_t *durations;
  uint8_t count;
  uint16_t gapMs;
};

const uint16_t bootNotes[] = {880, 1320, 1760};
const uint16_t bootDurs[] = {60, 60, 90};
const uint16_t sleepNotes[] = {700, 560, 420};
const uint16_t sleepDurs[] = {80, 90, 110};
const uint16_t wakeNotes[] = {660, 980, 1480};
const uint16_t wakeDurs[] = {60, 60, 90};
const uint16_t reactNotes[] = {988, 1319, 1175, 1760, 1480, 1047, 1568};
const uint16_t reactDurs[] = {65, 55, 55, 75, 65, 55, 85};

constexpr SequenceDefinition BOOT_SEQUENCE = {bootNotes, bootDurs, 3, 20};
constexpr SequenceDefinition SLEEP_SEQUENCE = {sleepNotes, sleepDurs, 3, 24};
constexpr SequenceDefinition WAKE_SEQUENCE = {wakeNotes, wakeDurs, 3, 20};
constexpr SequenceDefinition REACTION_SEQUENCE = {
    reactNotes, reactDurs,
    static_cast<uint8_t>(sizeof(reactNotes) / sizeof(reactNotes[0])), 18};

SoundSequence currentSequence = SoundSequence::None;
const SequenceDefinition *currentDefinition = nullptr;
uint32_t phaseEndsAt = 0;
uint8_t noteIndex = 0;
bool notePlaying = false;

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void stopSequence() {
  noTone(BUZZER_PIN);
  currentSequence = SoundSequence::None;
  currentDefinition = nullptr;
  phaseEndsAt = 0;
  noteIndex = 0;
  notePlaying = false;
}

void startSequence(SoundSequence sequence, const SequenceDefinition &definition,
                   uint32_t now) {
  stopSequence();
  currentSequence = sequence;
  currentDefinition = &definition;
  phaseEndsAt = now;
}
}  // namespace

void beginSoundEngine() {
  pinMode(BUZZER_PIN, OUTPUT);
  stopSequence();
}

void playBootSound() {
  startSequence(SoundSequence::Boot, BOOT_SEQUENCE, millis());
}

void playSleepSound() {
  startSequence(SoundSequence::Sleep, SLEEP_SEQUENCE, millis());
}

void playWakeSound() {
  startSequence(SoundSequence::Wake, WAKE_SEQUENCE, millis());
}

void startReactionSound(uint32_t now) {
  startSequence(SoundSequence::Reaction, REACTION_SEQUENCE, now);
}

void stopReactionSound() {
  if (currentSequence == SoundSequence::Reaction) stopSequence();
}

void updateSoundEngine(uint32_t now, BuddyReaction reaction) {
  if (currentSequence == SoundSequence::Reaction &&
      reaction != BuddyReaction::Generic) {
    stopSequence();
    return;
  }
  if (currentSequence == SoundSequence::None ||
      !timeReached(now, phaseEndsAt)) {
    return;
  }

  if (notePlaying) {
    noTone(BUZZER_PIN);
    notePlaying = false;
    phaseEndsAt = now + currentDefinition->gapMs;
    return;
  }

  if (noteIndex >= currentDefinition->count) {
    stopSequence();
    return;
  }

  tone(BUZZER_PIN, currentDefinition->notes[noteIndex]);
  phaseEndsAt = now + currentDefinition->durations[noteIndex];
  notePlaying = true;
  noteIndex++;
}

bool isSoundEngineActive() {
  return currentSequence != SoundSequence::None;
}

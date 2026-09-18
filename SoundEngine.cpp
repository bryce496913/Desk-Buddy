#include "SoundEngine.h"

#include <Arduino.h>
#include "Config.h"

namespace {
enum class SoundSequence : uint8_t {
  None,
  Boot,
  HappyReaction,
  CuriousReaction,
  AnnoyedReaction,
  StartledReaction,
  SuspiciousReaction,
  ConfusedReaction,
  Sleep,
  Wake
};

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
constexpr uint16_t happyNotes[] = {988, 1319, 1175, 1760, 1480, 1047, 1568};
constexpr uint16_t happyDurs[] = {65, 55, 55, 75, 65, 55, 85};
constexpr uint16_t happyBounceNotes[] = {784, 1175, 988, 1397, 1760};
constexpr uint16_t happyBounceDurs[] = {70, 65, 60, 70, 100};
constexpr uint16_t happyChirpNotes[] = {1047, 1319, 1568, 2093, 1760, 2349};
constexpr uint16_t happyChirpDurs[] = {45, 45, 50, 65, 50, 90};
constexpr uint16_t curiousRiseNotes[] = {740, 988, 1245, 1109};
constexpr uint16_t curiousRiseDurs[] = {75, 80, 105, 130};
constexpr uint16_t curiousTiltNotes[] = {932, 1175, 1047, 1397, 1245};
constexpr uint16_t curiousTiltDurs[] = {65, 75, 85, 100, 125};
constexpr uint16_t curiousQueryNotes[] = {659, 880, 1109, 988, 1319};
constexpr uint16_t curiousQueryDurs[] = {70, 70, 90, 90, 135};
constexpr uint16_t annoyedDropNotes[] = {784, 698, 587, 523};
constexpr uint16_t annoyedDropDurs[] = {80, 85, 95, 125};
constexpr uint16_t annoyedGrumbleNotes[] = {659, 622, 659, 554, 494};
constexpr uint16_t annoyedGrumbleDurs[] = {65, 65, 75, 90, 130};
constexpr uint16_t annoyedClipNotes[] = {740, 740, 622, 523};
constexpr uint16_t annoyedClipDurs[] = {55, 70, 85, 145};
constexpr uint16_t startledNotes[] = {2093, 784, 1568, 659};
constexpr uint16_t startledDurs[] = {55, 80, 50, 110};
constexpr uint16_t startledJumpNotes[] = {2349, 988, 1976, 1175, 2217};
constexpr uint16_t startledJumpDurs[] = {40, 65, 40, 60, 85};
constexpr uint16_t startledQuestionNotes[] = {1760, 2637, 1319, 2093};
constexpr uint16_t startledQuestionDurs[] = {45, 50, 70, 95};
constexpr uint16_t suspiciousQueryNotes[] = {740, 554};
constexpr uint16_t suspiciousQueryDurs[] = {120, 210};
constexpr uint16_t suspiciousProbeNotes[] = {659, 659, 494};
constexpr uint16_t suspiciousProbeDurs[] = {80, 90, 190};
constexpr uint16_t suspiciousGlanceNotes[] = {587, 466, 622};
constexpr uint16_t suspiciousGlanceDurs[] = {110, 140, 180};
constexpr uint16_t confusedQuestionNotes[] = {880, 1397, 740, 1047};
constexpr uint16_t confusedQuestionDurs[] = {85, 70, 135, 120};
constexpr uint16_t confusedWobbleNotes[] = {1047, 784, 1319, 698, 988};
constexpr uint16_t confusedWobbleDurs[] = {65, 90, 75, 125, 105};
constexpr uint16_t confusedStumbleNotes[] = {932, 1480, 1175, 659};
constexpr uint16_t confusedStumbleDurs[] = {90, 65, 80, 155};

constexpr SequenceDefinition BOOT_SEQUENCE = {bootNotes, bootDurs, 3, 20};
constexpr SequenceDefinition SLEEP_SEQUENCE = {sleepNotes, sleepDurs, 3, 24};
constexpr SequenceDefinition WAKE_SEQUENCE = {wakeNotes, wakeDurs, 3, 20};
constexpr SequenceDefinition HAPPY_REACTION_SEQUENCES[] = {
    {happyNotes, happyDurs,
     static_cast<uint8_t>(sizeof(happyNotes) / sizeof(happyNotes[0])), 18},
    {happyBounceNotes, happyBounceDurs,
     static_cast<uint8_t>(sizeof(happyBounceNotes) /
                          sizeof(happyBounceNotes[0])),
     22},
    {happyChirpNotes, happyChirpDurs,
     static_cast<uint8_t>(sizeof(happyChirpNotes) /
                          sizeof(happyChirpNotes[0])),
     16}};
constexpr SequenceDefinition STARTLED_REACTION_SEQUENCES[] = {
    {startledNotes, startledDurs,
     static_cast<uint8_t>(sizeof(startledNotes) / sizeof(startledNotes[0])), 15},
    {startledJumpNotes, startledJumpDurs,
     static_cast<uint8_t>(sizeof(startledJumpNotes) /
                          sizeof(startledJumpNotes[0])),
     12},
    {startledQuestionNotes, startledQuestionDurs,
     static_cast<uint8_t>(sizeof(startledQuestionNotes) /
                          sizeof(startledQuestionNotes[0])),
     14}};
constexpr SequenceDefinition CURIOUS_REACTION_SEQUENCES[] = {
    {curiousRiseNotes, curiousRiseDurs,
     static_cast<uint8_t>(sizeof(curiousRiseNotes) /
                          sizeof(curiousRiseNotes[0])),
     24},
    {curiousTiltNotes, curiousTiltDurs,
     static_cast<uint8_t>(sizeof(curiousTiltNotes) /
                          sizeof(curiousTiltNotes[0])),
     20},
    {curiousQueryNotes, curiousQueryDurs,
     static_cast<uint8_t>(sizeof(curiousQueryNotes) /
                          sizeof(curiousQueryNotes[0])),
     22}};
constexpr SequenceDefinition ANNOYED_REACTION_SEQUENCES[] = {
    {annoyedDropNotes, annoyedDropDurs,
     static_cast<uint8_t>(sizeof(annoyedDropNotes) /
                          sizeof(annoyedDropNotes[0])),
     24},
    {annoyedGrumbleNotes, annoyedGrumbleDurs,
     static_cast<uint8_t>(sizeof(annoyedGrumbleNotes) /
                          sizeof(annoyedGrumbleNotes[0])),
     18},
    {annoyedClipNotes, annoyedClipDurs,
     static_cast<uint8_t>(sizeof(annoyedClipNotes) /
                          sizeof(annoyedClipNotes[0])),
     20}};
constexpr SequenceDefinition SUSPICIOUS_REACTION_SEQUENCES[] = {
    {suspiciousQueryNotes, suspiciousQueryDurs,
     static_cast<uint8_t>(sizeof(suspiciousQueryNotes) /
                          sizeof(suspiciousQueryNotes[0])),
     90},
    {suspiciousProbeNotes, suspiciousProbeDurs,
     static_cast<uint8_t>(sizeof(suspiciousProbeNotes) /
                          sizeof(suspiciousProbeNotes[0])),
     65},
    {suspiciousGlanceNotes, suspiciousGlanceDurs,
     static_cast<uint8_t>(sizeof(suspiciousGlanceNotes) /
                          sizeof(suspiciousGlanceNotes[0])),
     75}};
constexpr SequenceDefinition CONFUSED_REACTION_SEQUENCES[] = {
    {confusedQuestionNotes, confusedQuestionDurs,
     static_cast<uint8_t>(sizeof(confusedQuestionNotes) /
                          sizeof(confusedQuestionNotes[0])),
     38},
    {confusedWobbleNotes, confusedWobbleDurs,
     static_cast<uint8_t>(sizeof(confusedWobbleNotes) /
                          sizeof(confusedWobbleNotes[0])),
     30},
    {confusedStumbleNotes, confusedStumbleDurs,
     static_cast<uint8_t>(sizeof(confusedStumbleNotes) /
                          sizeof(confusedStumbleNotes[0])),
     42}};

constexpr uint8_t HAPPY_VARIANT_COUNT =
    sizeof(HAPPY_REACTION_SEQUENCES) / sizeof(HAPPY_REACTION_SEQUENCES[0]);
constexpr uint8_t STARTLED_VARIANT_COUNT =
    sizeof(STARTLED_REACTION_SEQUENCES) /
    sizeof(STARTLED_REACTION_SEQUENCES[0]);
constexpr uint8_t CURIOUS_VARIANT_COUNT =
    sizeof(CURIOUS_REACTION_SEQUENCES) /
    sizeof(CURIOUS_REACTION_SEQUENCES[0]);
constexpr uint8_t ANNOYED_VARIANT_COUNT =
    sizeof(ANNOYED_REACTION_SEQUENCES) /
    sizeof(ANNOYED_REACTION_SEQUENCES[0]);
constexpr uint8_t SUSPICIOUS_VARIANT_COUNT =
    sizeof(SUSPICIOUS_REACTION_SEQUENCES) /
    sizeof(SUSPICIOUS_REACTION_SEQUENCES[0]);
constexpr uint8_t CONFUSED_VARIANT_COUNT =
    sizeof(CONFUSED_REACTION_SEQUENCES) /
    sizeof(CONFUSED_REACTION_SEQUENCES[0]);
static_assert(HAPPY_VARIANT_COUNT == 3, "Happy must have exactly three variants");
static_assert(STARTLED_VARIANT_COUNT == 3,
              "Startled must have exactly three variants");
static_assert(CURIOUS_VARIANT_COUNT == 3,
              "Curious must have exactly three variants");
static_assert(ANNOYED_VARIANT_COUNT == 3,
              "Annoyed must have exactly three variants");
static_assert(SUSPICIOUS_VARIANT_COUNT == 3,
              "Suspicious must have exactly three variants");
static_assert(CONFUSED_VARIANT_COUNT == 3,
              "Confused must have exactly three variants");

SoundSequence currentSequence = SoundSequence::None;
const SequenceDefinition *currentDefinition = nullptr;
uint32_t phaseEndsAt = 0;
uint8_t noteIndex = 0;
bool notePlaying = false;
uint8_t lastHappyVariant = UINT8_MAX;
uint8_t lastCuriousVariant = UINT8_MAX;
uint8_t lastAnnoyedVariant = UINT8_MAX;
uint8_t lastStartledVariant = UINT8_MAX;
uint8_t lastSuspiciousVariant = UINT8_MAX;
uint8_t lastConfusedVariant = UINT8_MAX;

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool isReactionSequence(SoundSequence sequence) {
  return sequence == SoundSequence::HappyReaction ||
         sequence == SoundSequence::CuriousReaction ||
         sequence == SoundSequence::AnnoyedReaction ||
         sequence == SoundSequence::StartledReaction ||
         sequence == SoundSequence::SuspiciousReaction ||
         sequence == SoundSequence::ConfusedReaction;
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

uint8_t selectVariant(uint8_t count, uint8_t &lastVariant) {
  uint8_t selected = static_cast<uint8_t>(random(count));
  if (count > 1 && selected == lastVariant) {
    selected = static_cast<uint8_t>((selected + 1 + random(count - 1)) % count);
  }
  lastVariant = selected;
  return selected;
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

void startReactionSound(uint32_t now, ReactionSound sound) {
  switch (sound) {
    case ReactionSound::None:
      stopReactionSound();
      break;
    case ReactionSound::Happy: {
      uint8_t variant = selectVariant(HAPPY_VARIANT_COUNT, lastHappyVariant);
      startSequence(SoundSequence::HappyReaction,
                    HAPPY_REACTION_SEQUENCES[variant], now);
      break;
    }
    case ReactionSound::Curious: {
      uint8_t variant = selectVariant(CURIOUS_VARIANT_COUNT, lastCuriousVariant);
      startSequence(SoundSequence::CuriousReaction,
                    CURIOUS_REACTION_SEQUENCES[variant], now);
      break;
    }
    case ReactionSound::Annoyed: {
      uint8_t variant = selectVariant(ANNOYED_VARIANT_COUNT, lastAnnoyedVariant);
      startSequence(SoundSequence::AnnoyedReaction,
                    ANNOYED_REACTION_SEQUENCES[variant], now);
      break;
    }
    case ReactionSound::Startled: {
      uint8_t variant =
          selectVariant(STARTLED_VARIANT_COUNT, lastStartledVariant);
      startSequence(SoundSequence::StartledReaction,
                    STARTLED_REACTION_SEQUENCES[variant], now);
      break;
    }
    case ReactionSound::Suspicious: {
      uint8_t variant =
          selectVariant(SUSPICIOUS_VARIANT_COUNT, lastSuspiciousVariant);
      startSequence(SoundSequence::SuspiciousReaction,
                    SUSPICIOUS_REACTION_SEQUENCES[variant], now);
      break;
    }
    case ReactionSound::Confused: {
      uint8_t variant =
          selectVariant(CONFUSED_VARIANT_COUNT, lastConfusedVariant);
      startSequence(SoundSequence::ConfusedReaction,
                    CONFUSED_REACTION_SEQUENCES[variant], now);
      break;
    }
  }
}

#if DESK_BUDDY_DIAGNOSTICS
bool startDiagnosticReactionSound(uint32_t now, ReactionSound sound,
                                  uint8_t requestedVariantIndex,
                                  uint8_t &selectedVariantIndex) {
  selectedVariantIndex = DIAGNOSTIC_RANDOM_VARIANT;

  const SequenceDefinition *sequences = nullptr;
  SoundSequence sequence = SoundSequence::None;
  uint8_t variantCount = 0;
  uint8_t *lastVariant = nullptr;

  switch (sound) {
    case ReactionSound::None:
      stopReactionSound();
      return true;
    case ReactionSound::Happy:
      sequences = HAPPY_REACTION_SEQUENCES;
      sequence = SoundSequence::HappyReaction;
      variantCount = HAPPY_VARIANT_COUNT;
      lastVariant = &lastHappyVariant;
      break;
    case ReactionSound::Curious:
      sequences = CURIOUS_REACTION_SEQUENCES;
      sequence = SoundSequence::CuriousReaction;
      variantCount = CURIOUS_VARIANT_COUNT;
      lastVariant = &lastCuriousVariant;
      break;
    case ReactionSound::Annoyed:
      sequences = ANNOYED_REACTION_SEQUENCES;
      sequence = SoundSequence::AnnoyedReaction;
      variantCount = ANNOYED_VARIANT_COUNT;
      lastVariant = &lastAnnoyedVariant;
      break;
    case ReactionSound::Startled:
      sequences = STARTLED_REACTION_SEQUENCES;
      sequence = SoundSequence::StartledReaction;
      variantCount = STARTLED_VARIANT_COUNT;
      lastVariant = &lastStartledVariant;
      break;
    case ReactionSound::Suspicious:
      sequences = SUSPICIOUS_REACTION_SEQUENCES;
      sequence = SoundSequence::SuspiciousReaction;
      variantCount = SUSPICIOUS_VARIANT_COUNT;
      lastVariant = &lastSuspiciousVariant;
      break;
    case ReactionSound::Confused:
      sequences = CONFUSED_REACTION_SEQUENCES;
      sequence = SoundSequence::ConfusedReaction;
      variantCount = CONFUSED_VARIANT_COUNT;
      lastVariant = &lastConfusedVariant;
      break;
  }

  if (requestedVariantIndex == DIAGNOSTIC_RANDOM_VARIANT) {
    selectedVariantIndex = selectVariant(variantCount, *lastVariant);
  } else {
    if (requestedVariantIndex >= variantCount) return false;
    selectedVariantIndex = requestedVariantIndex;
  }

  startSequence(sequence, sequences[selectedVariantIndex], now);
  return true;
}
#endif

void stopReactionSound() {
  if (isReactionSequence(currentSequence)) stopSequence();
}

void updateSoundEngine(uint32_t now, BuddyReaction reaction) {
  if (isReactionSequence(currentSequence) && reaction != BuddyReaction::Generic) {
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

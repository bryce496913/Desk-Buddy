#include <cassert>
#include <cstdint>

namespace {
long nextRandom = 0;
}

void pinMode(uint8_t, uint8_t) {}
void noTone(uint8_t) {}
void tone(uint8_t, unsigned int) {}
uint32_t millis() { return 0; }
long random(long maximum) { return nextRandom % maximum; }
long random(long minimum, long maximum) {
  return minimum + (nextRandom % (maximum - minimum));
}

#include "../SoundEngine.cpp"

int main() {
  uint8_t selected = DIAGNOSTIC_RANDOM_VARIANT;

  struct ReactionVariants {
    ReactionSound sound;
    const SequenceDefinition *sequences;
    uint8_t count;
    uint8_t *lastVariant;
  } reactions[] = {
      {ReactionSound::Happy, HAPPY_REACTION_SEQUENCES, HAPPY_VARIANT_COUNT,
       &lastHappyVariant},
      {ReactionSound::Curious, CURIOUS_REACTION_SEQUENCES,
       CURIOUS_VARIANT_COUNT, &lastCuriousVariant},
      {ReactionSound::Annoyed, ANNOYED_REACTION_SEQUENCES,
       ANNOYED_VARIANT_COUNT, &lastAnnoyedVariant},
      {ReactionSound::Startled, STARTLED_REACTION_SEQUENCES,
       STARTLED_VARIANT_COUNT, &lastStartledVariant},
      {ReactionSound::Suspicious, SUSPICIOUS_REACTION_SEQUENCES,
       SUSPICIOUS_VARIANT_COUNT, &lastSuspiciousVariant},
      {ReactionSound::Confused, CONFUSED_REACTION_SEQUENCES,
       CONFUSED_VARIANT_COUNT, &lastConfusedVariant},
  };

  for (const ReactionVariants &reaction : reactions) {
    for (uint8_t variant = 0; variant < reaction.count; variant++) {
      *reaction.lastVariant = UINT8_MAX;
      assert(startDiagnosticReactionSound(100, reaction.sound, variant,
                                          selected));
      assert(selected == variant);
      assert(currentDefinition == &reaction.sequences[variant]);
      assert(*reaction.lastVariant == UINT8_MAX);
    }

    const SequenceDefinition *beforeInvalid = currentDefinition;
    assert(!startDiagnosticReactionSound(101, reaction.sound, reaction.count,
                                         selected));
    assert(currentDefinition == beforeInvalid);

    // Random diagnostics use the production selector and its repeat avoidance.
    *reaction.lastVariant = 0;
    nextRandom = 0;
    assert(startDiagnosticReactionSound(102, reaction.sound,
                                        DIAGNOSTIC_RANDOM_VARIANT, selected));
    assert(selected != 0);
    assert(*reaction.lastVariant == selected);
  }

  assert(startDiagnosticReactionSound(103, ReactionSound::None, 2, selected));
  assert(selected == DIAGNOSTIC_RANDOM_VARIANT);
  assert(currentSequence == SoundSequence::None);
  return 0;
}

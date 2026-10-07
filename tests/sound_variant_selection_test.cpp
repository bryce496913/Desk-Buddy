#include <cassert>
#include <cstdint>
#include <initializer_list>
long ticket = 0;
long expectedPool = 0;
int draws = 0;
int tones = 0;
void pinMode(uint8_t, uint8_t) {}
void noTone(uint8_t) {}
void tone(uint8_t, unsigned int) { ++tones; }
uint32_t millis() { return 0; }
long random(long maximum) {
  assert(maximum == expectedPool && ticket >= 0 && ticket < maximum);
  ++draws;
  return ticket;
}
long random(long minimum, long maximum) { return minimum + random(maximum - minimum); }
// Exercise the actual private selector and playback definitions, without a public test API.
#include "../SoundEngine.cpp"
int main() {
  struct Family {
    ReactionSound sound;
    SoundSequence sequence;
    const SequenceDefinition* definitions;
    uint8_t* previous;
  } families[] = {
    {ReactionSound::Happy, SoundSequence::HappyReaction, HAPPY_REACTION_SEQUENCES, &lastHappyVariant},
    {ReactionSound::Curious, SoundSequence::CuriousReaction, CURIOUS_REACTION_SEQUENCES, &lastCuriousVariant},
    {ReactionSound::Annoyed, SoundSequence::AnnoyedReaction, ANNOYED_REACTION_SEQUENCES, &lastAnnoyedVariant},
    {ReactionSound::Startled, SoundSequence::StartledReaction, STARTLED_REACTION_SEQUENCES, &lastStartledVariant},
    {ReactionSound::Suspicious, SoundSequence::SuspiciousReaction, SUSPICIOUS_REACTION_SEQUENCES, &lastSuspiciousVariant},
    {ReactionSound::Confused, SoundSequence::ConfusedReaction, CONFUSED_REACTION_SEQUENCES, &lastConfusedVariant}
  };
  // Independent specification: six families, each with Calm/Engaged/Grumpy/Sleepy rows.
  const uint8_t expected[6][4][3] = {
    {{45,35,20},{25,30,45},{50,35,15},{55,35,10}},
    {{40,35,25},{45,30,25},{15,30,55},{15,55,30}},
    {{40,30,30},{35,20,45},{20,55,25},{45,40,15}},
    {{45,30,25},{25,45,30},{30,20,50},{30,55,15}},
    {{40,35,25},{25,45,30},{20,30,50},{35,20,45}},
    {{40,35,25},{45,35,20},{20,30,50},{20,50,30}}
  };
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    int familyIndex = 0;
    for (const auto& family : families) {
      const auto weights = weightsFor(family.sound, mood);
      for (int index = 0; index < 3; ++index) {
        assert(weights.values[index] == expected[familyIndex][static_cast<uint8_t>(mood)][index]);
        assert(weights.values[index] > 0);
      }
      assert(weights.values[0] + weights.values[1] + weights.values[2] == 100);
      for (uint8_t previous : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{UINT8_MAX}}) {
        int counts[3]{};
        int previousTicketResult = -1;
        expectedPool = previous == UINT8_MAX ? 100 : 100 - weights.values[previous];
        for (ticket = 0; ticket < expectedPool; ++ticket) {
          *family.previous = previous;
          const int before = draws;
          startReactionSound(100, family.sound, mood);
          assert(draws == before + 1);
          const uint8_t selected = *family.previous;
          assert(selected < 3 && selected != previous);
          ++counts[selected];
          // Nondecreasing ticket results plus exact counts prove every bucket boundary.
          assert(selected >= previousTicketResult);
          previousTicketResult = selected;
          assert(currentSequence == family.sequence && currentDefinition == &family.definitions[selected]);
#if DESK_BUDDY_DIAGNOSTICS
          *family.previous = previous;
          uint8_t diagnosticSelected = UINT8_MAX;
          assert(startDiagnosticReactionSound(100, family.sound, DIAGNOSTIC_RANDOM_VARIANT,
                                             diagnosticSelected, mood));
          assert(diagnosticSelected == selected && *family.previous == selected);
          assert(currentDefinition == &family.definitions[selected]);
#endif
        }
        for (uint8_t index = 0; index < 3; ++index)
          assert(counts[index] == (index == previous ? 0 : weights.values[index]));
      }
#if DESK_BUDDY_DIAGNOSTICS
      // Forced variants remain exact even when they repeat the previous selection.
      for (uint8_t forced = 0; forced < 3; ++forced) {
        *family.previous = forced;
        uint8_t selected = UINT8_MAX;
        const int before = draws;
        assert(startDiagnosticReactionSound(100, family.sound, forced, selected, mood));
        assert(selected == forced && draws == before);
        assert(currentDefinition == &family.definitions[forced]);
      }
#endif
      ++familyIndex;
    }
    const int before = draws;
    startReactionSound(100, ReactionSound::None, mood);
    updateSoundEngine(100, BuddyReaction::Generic);
    assert(!isSoundEngineActive() && draws == before && tones == 0);
  }
  // Exhaust all tickets of an unequal pool to prove integer weighting and exclusion.
  const VariantWeights uneven{{2, 3, 5}};
  for (uint8_t previous : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{UINT8_MAX}}) {
    expectedPool = previous < 3 ? 10 - uneven.values[previous] : 10;
    int counts[3]{};
    for (ticket = 0; ticket < expectedPool; ++ticket) {
      uint8_t last = previous;
      const auto selected = selectWeightedVariant(uneven, last);
      assert(selected < 3 && last == selected && selected != previous);
      ++counts[selected];
    }
    for (uint8_t index = 0; index < 3; ++index)
      assert(counts[index] == (index == previous ? 0 : uneven.values[index]));
  }
  // Sole eligible variant is allowed to repeat; zero-weight pools are bounded.
  expectedPool = 7; ticket = 6;
  uint8_t last = 1;
  assert(selectWeightedVariant(VariantWeights{{0, 7, 0}}, last) == 1);
  const int before = draws;
  assert(selectWeightedVariant(VariantWeights{{0, 0, 0}}, last) == 1 && draws == before);
  last = UINT8_MAX;
  assert(selectWeightedVariant(VariantWeights{{0, 0, 0}}, last) == 0 && draws == before);
  expectedPool = 765; ticket = 764; last = UINT8_MAX;
  assert(selectWeightedVariant(VariantWeights{{255, 255, 255}}, last) == 2);
  // Core sounds remain outside reaction selection and ignore None's reaction stop.
  playBootSound(); assert(currentDefinition == &BOOT_SEQUENCE);
  startReactionSound(100, ReactionSound::None, BuddyMood::Grumpy);
  assert(currentDefinition == &BOOT_SEQUENCE);
  playSleepSound(); assert(currentDefinition == &SLEEP_SEQUENCE);
  playWakeSound(); assert(currentDefinition == &WAKE_SEQUENCE);
}

#include <cassert>
#include <cstdint>
#include <string>
#include "FaceRenderer.h"
#include "Diagnostics.h"
long controlledTicket = 0;
int toneCalls = 0;
int stopCalls = 0;
unsigned int frequency = 0;
void pinMode(uint8_t, uint8_t) {}
void noTone(uint8_t) { ++stopCalls; frequency = 0; }
void tone(uint8_t, unsigned int value) { ++toneCalls; frequency = value; }
uint32_t millis() { return 0; }
long random(long maximum) { assert(controlledTicket < maximum); return controlledTicket; }
long random(long minimum, long maximum) { return minimum + random(maximum - minimum); }
#include "../SoundEngine.cpp"
HardwareSerial Serial;
FaceExpression face = FaceExpression::Normal;
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression expression) { face = expression; }
bool isFaceReactionFinished(uint32_t) { return false; }
void finishFaceReaction(uint32_t) { face = FaceExpression::Normal; }
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}
void ignoreSoundSensorAfterWake() {}
void command(const char* text, uint32_t now = 100) {
  for (const char* p = text; *p; ++p) { Serial.push(*p); updateDiagnostics(now); }
}
void equalMood(const DiagnosticMoodState& a, const DiagnosticMoodState& b) {
  assert(a.mood == b.mood && a.engagementScore == b.engagementScore &&
         a.irritationScore == b.irritationScore && a.inactivityMs == b.inactivityMs);
}
int main() {
  beginSoundEngine(); beginBehaviorEngine(100);
  command("s?"); assert(Serial.output == "DIAG: Sound = None\n");
  Serial.clearOutput();
  Serial.push('s'); updateDiagnostics(100); updateDiagnostics(101);
  assert(Serial.output.empty()); // Partial command does not block or print.
  command("x"); assert(Serial.output == "DIAG: Invalid sound command (use s? or sw)\n");
  command("mgvr3");
  const auto selected = getDiagnosticSoundSelection();
  assert(selected.valid && selected.sound == ReactionSound::Annoyed && selected.mood == BuddyMood::Grumpy);
  Serial.clearOutput(); command("s?");
  assert(Serial.output == "DIAG: Sound = Annoyed | Mood = Grumpy | Variant = 1\n");
  const auto beforeQuery = getDiagnosticMoodState(100);
  const auto memory = getDiagnosticRecentInteractionContext(100);
  const auto previous = lastAnnoyedVariant;
  Serial.clearOutput(); command("sw");
  assert(Serial.output == "DIAG SOUND WEIGHTS - Grumpy\nHappy 50 / 35 / 15\nCurious 15 / 30 / 55\nAnnoyed 20 / 55 / 25\nStartled 30 / 20 / 50\nSuspicious 20 / 30 / 50\nConfused 20 / 30 / 50\n");
  equalMood(beforeQuery, getDiagnosticMoodState(100));
  const auto afterMemory = getDiagnosticRecentInteractionContext(100);
  assert(memory.type == afterMemory.type && memory.ageMs == afterMemory.ageMs && memory.recent == afterMemory.recent);
  assert(lastAnnoyedVariant == previous);
  command("s \n?v23v23");
  assert(currentDefinition == &ANNOYED_REACTION_SEQUENCES[1]);
  assert(lastAnnoyedVariant == previous);
  command("vr3"); assert(lastAnnoyedVariant != previous);
  // Same ticket, no previous variant: Calm Happy base vs Engaged Happy Bounce.
  controlledTicket = 30; lastHappyVariant = UINT8_MAX;
  command("mcvr1"); assert(getDiagnosticSoundSelection().variantIndex == 0);
  lastHappyVariant = UINT8_MAX;
  command("me1"); assert(getDiagnosticSoundSelection().variantIndex == 1);
  assert(getDiagnosticSoundSelection().mood == BuddyMood::Engaged);
  // Variant selection consumes mood without writing scores or context.
  const auto beforeSelection = getDiagnosticMoodState(100);
  startReactionSound(100, ReactionSound::Happy, BuddyMood::Grumpy);
  equalMood(beforeSelection, getDiagnosticMoodState(100));
  // Real interaction retains its pre-event mood in selection telemetry.
  controlledTicket = 40; lastStartledVariant = UINT8_MAX;
  beginBehaviorEngine(100); setDiagnosticMood(BuddyMood::Sleepy, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 110);
  assert(getBuddyMood() == BuddyMood::Calm && face == FaceExpression::Startled);
  const auto arrival = getDiagnosticSoundSelection();
  assert(arrival.valid && arrival.mood == BuddyMood::Sleepy && arrival.variantIndex == 1);
  Serial.clearOutput(); command("s?", 110);
  assert(Serial.output == "DIAG: Sound = Startled | Mood = Sleepy | Variant = 2\n");
  controlledTicket = 0; command("a8", 110);
  assert(!getDiagnosticSoundSelection().valid && !isSoundEngineActive());
  Serial.clearOutput(); command("s?", 110); assert(Serial.output == "DIAG: Sound = None\n");
  // Representative actual sequence lifecycle, at note/gap boundaries, no blocking.
  lastHappyVariant = UINT8_MAX;
  startReactionSound(1000, ReactionSound::Happy, BuddyMood::Calm);
  const auto* definition = currentDefinition;
  const int callsBefore = toneCalls;
  assert(!notePlaying && noteIndex == 0 && phaseEndsAt == 1000);
  updateSoundEngine(1000, BuddyReaction::Generic);
  assert(toneCalls == callsBefore + 1 && notePlaying && frequency == definition->notes[0]);
  const uint32_t firstEnd = phaseEndsAt;
  updateSoundEngine(firstEnd - 1, BuddyReaction::Generic); assert(notePlaying);
  updateSoundEngine(firstEnd, BuddyReaction::Generic);
  assert(!notePlaying && noteIndex == 1 && frequency == 0);
  assert(phaseEndsAt == firstEnd + definition->gapMs);
  updateSoundEngine(phaseEndsAt - 1, BuddyReaction::Generic); assert(toneCalls == callsBefore + 1);
  updateSoundEngine(phaseEndsAt, BuddyReaction::Generic);
  assert(notePlaying && noteIndex == 2 && frequency == definition->notes[1]);
  // Each call advances a single phase; bounded host traversal to completion.
  for (int phase = 0; phase < 2 * definition->count + 1 && isSoundEngineActive(); ++phase)
    updateSoundEngine(phaseEndsAt, BuddyReaction::Generic);
  assert(!isSoundEngineActive() && frequency == 0);
  assert(getDiagnosticSoundSelection().valid); // Completion preserves last selection.
  const auto historyAfterPlayback = lastHappyVariant;
  beginSoundEngine(); assert(!getDiagnosticSoundSelection().valid);
  assert(lastHappyVariant == historyAfterPlayback); // Telemetry reset does not reset production history.
}

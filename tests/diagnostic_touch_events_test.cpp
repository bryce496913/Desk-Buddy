#include <cassert>
#include <cstdint>
#include <string>
#include "FaceRenderer.h"
#include "Diagnostics.h"
long controlledTicket = 0;
int toneCalls = 0;
int randomCalls = 0;
int faceStarts = 0;
int stopCalls = 0;
unsigned int frequency = 0;
void pinMode(uint8_t, uint8_t) {}
void noTone(uint8_t) { ++stopCalls; frequency = 0; }
void tone(uint8_t, unsigned int value) { ++toneCalls; frequency = value; }
uint32_t millis() { return 0; }
long random(long maximum) { ++randomCalls; assert(controlledTicket < maximum); return controlledTicket; }
long random(long minimum, long maximum) { return minimum + random(maximum - minimum); }
#include "../SoundEngine.cpp"
HardwareSerial Serial;
FaceExpression face = FaceExpression::Normal;
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression expression) { ++faceStarts; face = expression; }
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
void reset() {
  beginSoundEngine(); beginBehaviorEngine(100);
  controlledTicket = 0; randomCalls = 0; faceStarts = 0; Serial.clearOutput();
  lastHappyVariant = lastCuriousVariant = lastAnnoyedVariant = lastStartledVariant =
      lastSuspiciousVariant = lastConfusedVariant = UINT8_MAX;
  face = FaceExpression::Normal;
}
struct Snapshot {
  BuddyReaction reaction;
  DiagnosticMoodState mood;
  DiagnosticRecentInteractionContext memory;
  DiagnosticAutonomousTimingState schedule;
  DiagnosticSoundSelection selection;
  FaceExpression expression;
  bool audio;
  int draws;
};
Snapshot snapshot(uint32_t now) {
  return {getBuddyReaction(), getDiagnosticMoodState(now), getDiagnosticRecentInteractionContext(now),
          getDiagnosticAutonomousTimingState(now), getDiagnosticSoundSelection(), face,
          isSoundEngineActive(), randomCalls};
}
void equalSnapshot(const Snapshot& a, const Snapshot& b) {
  assert(a.reaction == b.reaction && a.expression == b.expression && a.audio == b.audio && a.draws == b.draws);
  equalMood(a.mood, b.mood);
  assert(a.memory.type == b.memory.type && a.memory.ageMs == b.memory.ageMs && a.memory.recent == b.memory.recent);
  assert(a.schedule.scheduled == b.schedule.scheduled && a.schedule.scheduleMood == b.schedule.scheduleMood &&
         a.schedule.remainingMs == b.schedule.remainingMs && a.schedule.minMs == b.schedule.minMs && a.schedule.maxMs == b.schedule.maxMs);
  assert(a.selection.valid == b.selection.valid && a.selection.sound == b.selection.sound &&
         a.selection.mood == b.selection.mood && a.selection.variantIndex == b.selection.variantIndex);
}
int main() {
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    for (BuddySoundMode mode : {BuddySoundMode::Normal, BuddySoundMode::Quiet}) {
      for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
        for (bool afterSound : {false, true}) {
          reset(); setDiagnosticMood(mood, 100); setDiagnosticSoundMode(mode);
          if (afterSound) processBuddyEvent(BuddyEvent::SoundDetected, 105);
          processBuddyEvent(event, 110);
          const auto direct = snapshot(110);
          reset(); setDiagnosticMood(mood, 100); setDiagnosticSoundMode(mode);
          if (afterSound) processBuddyEvent(BuddyEvent::SoundDetected, 105);
          command(event == BuddyEvent::TouchTap ? "et" : "eh", 110);
          equalSnapshot(snapshot(110), direct);
          assert(Serial.output == (event == BuddyEvent::TouchTap ? "DIAG EVENT: TouchTap\n" : "DIAG EVENT: TouchHold\n"));
        }
      }
    }
  }
  reset(); command("mcet", 110);
  assert(face == FaceExpression::Happy && getBuddyMood() == BuddyMood::Calm);
  assert(getDiagnosticMoodState(110).engagementScore == 20);
  command("et", 111);
  assert(face == FaceExpression::Curious && getBuddyMood() == BuddyMood::Engaged);
  assert(getDiagnosticMoodState(111).engagementScore == 45);
  assert(getDiagnosticRecentInteractionContext(111).type == DiagnosticRecentInteractionType::TouchTap);
  command("et", 112); assert(face == FaceExpression::Curious);
  assert(getDiagnosticMoodState(112).irritationScore == 25);

  reset(); command("et", 110); command("eh", 111); command("et", 112);
  assert(face == FaceExpression::Happy); // Second Tap, not third: Hold has its own history.
  assert(getDiagnosticMoodState(112).engagementScore == 75 && getDiagnosticMoodState(112).irritationScore == 0);
  reset(); command("mg", 100); command("eh", 110);
  assert(face == FaceExpression::Curious);
  assert(getDiagnosticMoodState(110).engagementScore == 30 && getDiagnosticMoodState(110).irritationScore == 45);
  assert(getDiagnosticRecentInteractionContext(110).type == DiagnosticRecentInteractionType::TouchHold);

  reset(); command("et", 110); processBuddyEvent(BuddyEvent::SoundDetected, 111);
  assert(face == FaceExpression::Confused); // Simulated Tap is real cross-interaction context.
  reset(); processBuddyEvent(BuddyEvent::SoundDetected, 110); command("eh", 111);
  assert(face == FaceExpression::Happy && getDiagnosticRecentInteractionContext(111).type == DiagnosticRecentInteractionType::TouchHold);

  reset(); command("qqet", 110); command("eh", 111);
  assert(getBuddyReaction() == BuddyReaction::Generic && faceStarts == 2);
  assert(!isSoundEngineActive() && randomCalls == 0 && getBuddySoundMode() == BuddySoundMode::Quiet);
  assert(getDiagnosticRecentInteractionContext(111).type == DiagnosticRecentInteractionType::TouchHold);

  reset(); command("et", 110);
  const int starts = faceStarts;
  const auto stateBeforeShowcase = getDiagnosticMoodState(111);
  const auto memoryBeforeShowcase = getDiagnosticRecentInteractionContext(111);
  command("6", 111); // Direct showcase still does not record an interaction.
  equalMood(stateBeforeShowcase, getDiagnosticMoodState(111));
  const auto memoryAfterShowcase = getDiagnosticRecentInteractionContext(111);
  assert(memoryBeforeShowcase.type == memoryAfterShowcase.type && memoryBeforeShowcase.ageMs == memoryAfterShowcase.ageMs);
  command("eh", 112); assert(faceStarts == starts + 2 && face == FaceExpression::Happy);

  reset(); processBuddyEvent(BuddyEvent::ButtonShortPress, 110);
  const auto sleeping = snapshot(111); const int startsWhileSleeping = faceStarts;
  Serial.clearOutput(); command("eteh", 111);
  equalSnapshot(snapshot(111), sleeping);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping && faceStarts == startsWhileSleeping);
  assert(Serial.output == "DIAG: Event ignored while sleeping\nDIAG: Event ignored while sleeping\n");
  processBuddyEvent(BuddyEvent::ButtonShortPress, 112);
  command("et", 113); assert(face == FaceExpression::Happy);
}

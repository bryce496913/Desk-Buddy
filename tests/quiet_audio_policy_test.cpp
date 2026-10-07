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
}
void productionFamily(int index) {
  if (index <= 2) {
    for (int tap = 0; tap <= index; ++tap) {
      setDiagnosticMood(BuddyMood::Calm, 100 + tap);
      processBuddyEvent(BuddyEvent::TouchTap, 100 + tap);
    }
  } else {
    for (int sound = 0; sound <= index - 3; ++sound) {
      setDiagnosticMood(BuddyMood::Calm, 100 + sound);
      processBuddyEvent(BuddyEvent::SoundDetected, 100 + sound);
    }
  }
}
int main() {
  const FaceExpression expected[] = {FaceExpression::Happy, FaceExpression::Curious,
      FaceExpression::Annoyed, FaceExpression::Startled, FaceExpression::Suspicious, FaceExpression::Confused};
  // All six categories retain identical face policy, without invoking audio selection in Quiet.
  for (int family = 0; family < 6; ++family) {
    reset(); productionFamily(family);
    assert(face == expected[family] && isSoundEngineActive() && randomCalls > 0);
    const auto normalMood = getDiagnosticMoodState(102);
    reset(); setDiagnosticSoundMode(BuddySoundMode::Quiet);
    const uint8_t history[] = {lastHappyVariant,lastCuriousVariant,lastAnnoyedVariant,
                              lastStartledVariant,lastSuspiciousVariant,lastConfusedVariant};
    const int beforeTones = toneCalls;
    productionFamily(family);
    updateSoundEngine(103, getBuddyReaction());
    assert(face == expected[family] && getBuddyReaction() == BuddyReaction::Generic);
    assert(!isSoundEngineActive() && currentDefinition == nullptr && randomCalls == 0 && toneCalls == beforeTones);
    const uint8_t after[] = {lastHappyVariant,lastCuriousVariant,lastAnnoyedVariant,
                            lastStartledVariant,lastSuspiciousVariant,lastConfusedVariant};
    for (int index = 0; index < 6; ++index) assert(history[index] == after[index]);
    equalMood(normalMood, getDiagnosticMoodState(102));
  }
  reset(); processBuddyEvent(BuddyEvent::TouchHold, 110);
  assert(face == FaceExpression::Happy);
  updateSoundEngine(110, getBuddyReaction()); assert(frequency != 0);
  const int starts = faceStarts; const int draws = randomCalls;
  processBuddyEvent(BuddyEvent::ButtonLongPress, 111);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet && !isSoundEngineActive() && frequency == 0);
  assert(getBuddyReaction() == BuddyReaction::Generic && face == FaceExpression::Happy && faceStarts == starts);
  processBuddyEvent(BuddyEvent::ButtonLongPress, 112);
  assert(getBuddySoundMode() == BuddySoundMode::Normal && !isSoundEngineActive());
  assert(randomCalls == draws && faceStarts == starts);
  processBuddyEvent(BuddyEvent::TouchHold, 113); assert(isSoundEngineActive() && randomCalls == draws + 1);

  // Quiet interactions preserve family-local history across a return to Normal.
  reset(); lastHappyVariant = UINT8_MAX; controlledTicket = 50;
  processBuddyEvent(BuddyEvent::TouchHold, 110); assert(lastHappyVariant == 1);
  processBuddyEvent(BuddyEvent::ButtonLongPress, 111);
  const int beforeSilent = randomCalls;
  for (uint32_t now = 112; now <= 114; ++now) processBuddyEvent(BuddyEvent::TouchHold, now);
  assert(randomCalls == beforeSilent && lastHappyVariant == 1);
  processBuddyEvent(BuddyEvent::ButtonLongPress, 115);
  assert(!isSoundEngineActive() && randomCalls == beforeSilent);
  controlledTicket = 0; processBuddyEvent(BuddyEvent::TouchHold, 116);
  assert(lastHappyVariant != 1 && randomCalls == beforeSilent + 1);

  // Boot/Sleep/Wake remain permitted and survive preference transitions.
  reset(); playBootSound(); processBuddyEvent(BuddyEvent::ButtonLongPress, 101);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet && currentDefinition == &BOOT_SEQUENCE);
  processBuddyEvent(BuddyEvent::ButtonShortPress, 102);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping && currentDefinition == &SLEEP_SEQUENCE);
  processBuddyEvent(BuddyEvent::ButtonShortPress, 103);
  assert(getBuddyCoreState() == BuddyCoreState::Awake && currentDefinition == &WAKE_SEQUENCE);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet);
  // Finish the permitted wake cue before checking a silent production reaction.
  stopSequence(); processBuddyEvent(BuddyEvent::TouchTap, 104); assert(!isSoundEngineActive());
  processBuddyEvent(BuddyEvent::ButtonShortPress, 105);
  processBuddyEvent(BuddyEvent::ButtonLongPress, 106);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet && currentDefinition == &WAKE_SEQUENCE);
  beginBehaviorEngine(107); assert(getBuddySoundMode() == BuddySoundMode::Normal);

  // Cross-interaction policy remains visual even when all its audio is suppressed.
  reset(); command("qq");
  processBuddyEvent(BuddyEvent::TouchTap, 110);
  processBuddyEvent(BuddyEvent::SoundDetected, 111);
  assert(face == FaceExpression::Confused && !isSoundEngineActive() && randomCalls == 0);
  processBuddyEvent(BuddyEvent::TouchHold, 112);
  assert(face == FaceExpression::Happy && !isSoundEngineActive() && randomCalls == 0);

  // Explicit forced and random diagnostic auditions bypass Quiet for all six families.
  for (const char* reaction : {"1","2","3","4","5","6"}) {
    reset(); command("qq");
    Serial.clearOutput(); command("q?"); assert(Serial.output == "DIAG: Sound Mode = Quiet\n");
    for (const char* variant : {"v1","v2","v3"}) {
      command(variant); command(reaction);
      assert(isSoundEngineActive() && getBuddySoundMode() == BuddySoundMode::Quiet && randomCalls == 0);
    }
    const int beforeRandom = randomCalls;
    command("vr"); command(reaction);
    assert(isSoundEngineActive() && randomCalls == beforeRandom + 1);
    command("qn"); assert(getBuddySoundMode() == BuddySoundMode::Normal);
  }
  // Diagnostic forcing obeys the same immediate-stop/no-retroactive-play policy.
  reset(); command("1"); updateSoundEngine(100, getBuddyReaction()); assert(frequency != 0);
  command("qq"); assert(!isSoundEngineActive() && frequency == 0 && face == FaceExpression::Happy);
  const int beforeNormal = randomCalls;
  command("qn"); assert(!isSoundEngineActive() && randomCalls == beforeNormal);
  command("1"); assert(isSoundEngineActive());
}

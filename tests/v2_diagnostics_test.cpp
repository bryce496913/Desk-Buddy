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
void inspect(uint32_t now) {
  const auto mood = getDiagnosticMoodState(now);
  const auto memory = getDiagnosticRecentInteractionContext(now);
  const int draws = randomCalls; const int starts = faceStarts;
  Serial.clearOutput(); command("d?", now);
  assert(Serial.output.find("DIAG STATE\n") == 0);
  assert(Serial.output.find(getBuddyCoreState() == BuddyCoreState::Awake ? "Core = Awake\n" : "Core = Sleeping\n") != std::string::npos);
  assert(Serial.output.find(getBuddyReaction() == BuddyReaction::Generic ? "Reaction = Generic\n" : "Reaction = Idle\n") != std::string::npos);
  assert(Serial.output.find("Autonomous = Disabled in diagnostics | Mood =") != std::string::npos);
  assert(Serial.output.find(" | Range =") != std::string::npos);
  equalMood(mood, getDiagnosticMoodState(now));
  const auto afterMemory = getDiagnosticRecentInteractionContext(now);
  assert(memory.type == afterMemory.type && memory.ageMs == afterMemory.ageMs && memory.recent == afterMemory.recent);
  assert(randomCalls == draws && faceStarts == starts);
}
int main() {
  reset(); command("?", 100);
  for (const char* group : {"V2 state:", "Production event simulation:", "Autonomous showcase", "Reaction showcase", "Sound diagnostics:"})
    assert(Serial.output.find(group) != std::string::npos);
  const char* modes[] = {"mc", "me", "mg", "ms"};
  const BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy};
  const char* names[] = {"Calm", "Engaged", "Grumpy", "Sleepy"};
  for (int index = 0; index < 4; ++index) {
    command(modes[index], 100);
    assert(getBuddyMood() == moods[index]);
    Serial.clearOutput(); command("m?", 100);
    assert(Serial.output.find(std::string("DIAG: Mood = ") + names[index]) == 0);
    inspect(100); assert(Serial.output.find(std::string("Mood = ") + names[index] + "\n") != std::string::npos);
  }
  reset(); command("mcet", 110); inspect(115);
  assert(Serial.output == "DIAG STATE\nCore = Awake\nReaction = Generic\nMood = Calm\nEngagement = 20\nIrritation = 0\nInactive = 5 ms\nSound Mode = Normal\nInteraction = TouchTap | Age = 5 ms | Recent = Yes\nAutonomous = Disabled in diagnostics | Mood = Calm | Range = 28000-45000 ms\n");
  reset(); command("mg", 100); const auto beforeHold = getDiagnosticMoodState(110);
  command("eh", 110); inspect(115);
  assert(getDiagnosticRecentInteractionContext(115).type == DiagnosticRecentInteractionType::TouchHold);
  assert(getDiagnosticMoodState(115).engagementScore > beforeHold.engagementScore);
  assert(getDiagnosticMoodState(115).irritationScore < beforeHold.irritationScore);
  assert(Serial.output.find("Interaction = TouchHold | Age = 5 ms | Recent = Yes") != std::string::npos);

  reset(); command("me", 100);
  const auto beforeAutonomy = getDiagnosticMoodState(110);
  command("ea", 110); assert(face == FaceExpression::ExcitedScanning);
  equalMood(beforeAutonomy, getDiagnosticMoodState(110));
  inspect(110); assert(Serial.output.find("Interaction = None | Age = 0 ms | Recent = No") != std::string::npos);
  const auto beforeShowcase = getDiagnosticMoodState(111);
  command("a7", 111); assert(face == FaceExpression::SleepyDrift);
  equalMood(beforeShowcase, getDiagnosticMoodState(111));
  assert(getDiagnosticRecentInteractionContext(111).type == DiagnosticRecentInteractionType::None);
  command("ea", 112); assert(face == FaceExpression::Curious); // Still excludes prior production ExcitedScanning.

  reset(); command("et", 110); processBuddyEvent(BuddyEvent::SoundDetected, 111);
  assert(face == FaceExpression::Confused);
  assert(getDiagnosticSoundSelection().sound == ReactionSound::Confused);
  inspect(5112); // Snapshot reports aged interaction, without erasing memory.
  assert(Serial.output.find("Interaction = Sound | Age = 5001 ms | Recent = No") != std::string::npos);
  assert(getDiagnosticRecentInteractionContext(5112).type == DiagnosticRecentInteractionType::Sound);

  reset(); command("qqet", 110);
  assert(face == FaceExpression::Happy && getBuddyReaction() == BuddyReaction::Generic && !isSoundEngineActive());
  inspect(110); assert(Serial.output.find("Sound Mode = Quiet\n") != std::string::npos);
  command("qnet", 111); assert(isSoundEngineActive() && face == FaceExpression::Curious);
  inspect(111); assert(Serial.output.find("Sound Mode = Normal\n") != std::string::npos);

  processBuddyEvent(BuddyEvent::ButtonShortPress, 112);
  const auto beforeSleepEvents = getDiagnosticMoodState(113);
  const int sleepingStarts = faceStarts;
  command("etehea", 113);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping && faceStarts == sleepingStarts);
  equalMood(beforeSleepEvents, getDiagnosticMoodState(113));
  inspect(113); assert(Serial.output.find("Inactive = 0 ms\n") != std::string::npos);
  command("ms", 113); assert(getBuddyMood() == BuddyMood::Sleepy); // Existing sleeping mood-control contract.
  inspect(113); assert(Serial.output.find("Mood = Sleepy\n") != std::string::npos);

  reset(); Serial.clearOutput(); command("exd?dxd?mxd?axd?", 110);
  for (const char* failure : {"Invalid event command", "Invalid state command", "Invalid mood command", "Invalid autonomous command"})
    assert(Serial.output.find(failure) != std::string::npos);
  size_t position = 0; int snapshots = 0;
  while ((position = Serial.output.find("DIAG STATE\n", position)) != std::string::npos) { ++snapshots; position += 11; }
  assert(snapshots == 4);
  // Rapid tuning: no wait loops or synthetic clock advancement are needed.
  reset(); command("mg", 100); inspect(100);
  command("et", 101); inspect(101);
  command("eh", 102); inspect(102);
  const auto beforeRapidAuto = getDiagnosticMoodState(103);
  const auto rapidMemory = getDiagnosticRecentInteractionContext(103);
  command("ea", 103); inspect(103);
  equalMood(beforeRapidAuto, getDiagnosticMoodState(103));
  assert(getDiagnosticRecentInteractionContext(103).type == rapidMemory.type);
  // Individual state commands remain available.
  command("m?i?t?q?s?", 103);
  assert(Serial.output.find("DIAG: Mood =") != std::string::npos);
  assert(Serial.output.find("DIAG: Interaction =") != std::string::npos);
  assert(Serial.output.find("DIAG: Sound Mode =") != std::string::npos);
}

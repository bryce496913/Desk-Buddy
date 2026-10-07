#include <cassert>
#include <cstdint>
#include <string>

#include "Arduino.h"
#include "Diagnostics.h"
#include "SoundEngine.h"
#include "BehaviorEngine.h"

HardwareSerial Serial;

namespace {
DiagnosticReaction lastReaction = DiagnosticReaction::Normal;
DiagnosticSoundVariant lastVariant = DiagnosticSoundVariant::Random;
int triggerCount = 0;
BuddyMood currentMood = BuddyMood::Calm;
int moodSetCount = 0;
uint32_t lastMoodSetAt = 0;
uint32_t lastTelemetryAt = 0;
int telemetryReadCount = 0;
DiagnosticRecentInteractionContext interaction = {
    DiagnosticRecentInteractionType::None, 0, false};
int interactionReadCount = 0;
uint32_t interactionReadAt = 0;
BuddyCoreState coreState = BuddyCoreState::Awake;
DiagnosticAutonomousBehavior lastAutonomous = DiagnosticAutonomousBehavior::Curious;
int autonomousTriggerCount = 0;
DiagnosticAutonomousTimingState timing = {false, BuddyMood::Calm, 0, 28000, 45000};
int timingReadCount = 0;
uint32_t timingReadAt = 0;

void sendCommand(const char* command) {
  for (const char* cursor = command; *cursor; ++cursor) {
    Serial.push(*cursor);
    updateDiagnostics(200);
  }
}
}

BuddyCoreState getBuddyCoreState() { return coreState; }
DiagnosticAutonomousTimingState getDiagnosticAutonomousTimingState(uint32_t now) {
  ++timingReadCount;
  timingReadAt = now;
  return timing;
}
bool triggerDiagnosticAutonomousBehavior(DiagnosticAutonomousBehavior behavior,
                                         uint32_t) {
  if (coreState == BuddyCoreState::Sleeping) return false;
  lastAutonomous = behavior;
  ++autonomousTriggerCount;
  return true;
}
BuddyMood getBuddyMood() { return currentMood; }
DiagnosticRecentInteractionContext getDiagnosticRecentInteractionContext(uint32_t now) {
  ++interactionReadCount;
  interactionReadAt = now;
  return interaction;
}
DiagnosticMoodState getDiagnosticMoodState(uint32_t now) {
  lastTelemetryAt = now;
  telemetryReadCount++;
  return {currentMood, 45, 7,
          coreState == BuddyCoreState::Sleeping ? uint32_t{0} : uint32_t{3200}};
}
void setDiagnosticMood(BuddyMood mood, uint32_t now) {
  lastMoodSetAt = now;
  currentMood = mood;
  moodSetCount++;
}

bool triggerDiagnosticReaction(DiagnosticReaction reaction,
                               DiagnosticSoundVariant variant, uint32_t,
                               uint8_t &selectedVariantIndex) {
  lastReaction = reaction;
  lastVariant = variant;
  triggerCount++;
  selectedVariantIndex = reaction == DiagnosticReaction::Daydreaming ||
                                 reaction == DiagnosticReaction::Normal
      ? UINT8_MAX
      : (variant == DiagnosticSoundVariant::Random
             ? 2
             : static_cast<uint8_t>(variant) - 1);
  return true;
}

int main() {
  beginDiagnostics();
  assert(Serial.output.find("v1: Variant 1") != std::string::npos);
  assert(Serial.output.find("Sound variant mode: Random") !=
         std::string::npos);
  assert(Serial.output.find("m?: Current mood") != std::string::npos);
  assert(Serial.output.find("mc: Calm") != std::string::npos);
  assert(Serial.output.find("me: Engaged") != std::string::npos);
  assert(Serial.output.find("mg: Grumpy") != std::string::npos);
  assert(Serial.output.find("ms: Sleepy") != std::string::npos);
  assert(Serial.output.find("i?: Recent interaction") != std::string::npos);
  Serial.clearOutput();

  // The first character only changes parser state; it never waits or triggers.
  Serial.push('v');
  updateDiagnostics(100);
  assert(triggerCount == 0);

  Serial.push('2');
  updateDiagnostics(101);
  assert(triggerCount == 0);
  assert(Serial.output.find("Sound variant = Variant 2") !=
         std::string::npos);

  Serial.push('4');
  updateDiagnostics(102);
  assert(triggerCount == 1);
  assert(lastReaction == DiagnosticReaction::Startled);
  assert(lastVariant == DiagnosticSoundVariant::Variant2);
  assert(Serial.output.find("Startled / Variant 2") != std::string::npos);

  Serial.push('4');
  updateDiagnostics(103);
  assert(triggerCount == 2);
  assert(lastVariant == DiagnosticSoundVariant::Variant2);

  Serial.push('v');
  updateDiagnostics(104);
  Serial.push('r');
  updateDiagnostics(105);
  Serial.push('1');
  updateDiagnostics(106);
  assert(lastReaction == DiagnosticReaction::Happy);
  assert(lastVariant == DiagnosticSoundVariant::Random);
  assert(Serial.output.find("Happy / Variant 3") != std::string::npos);

  Serial.push('v');
  updateDiagnostics(107);
  Serial.push('3');
  updateDiagnostics(108);
  Serial.push('7');
  updateDiagnostics(109);
  assert(lastReaction == DiagnosticReaction::Daydreaming);
  assert(Serial.output.find("Daydreaming / Silent") != std::string::npos);

  Serial.push('0');
  updateDiagnostics(110);
  assert(lastReaction == DiagnosticReaction::Normal);
  assert(lastVariant == DiagnosticSoundVariant::Variant3);

  const int triggersBeforeMood = triggerCount;
  Serial.clearOutput();
  sendCommand("m?");
  assert(Serial.output == "DIAG: Mood = Calm | Engagement = 45 | Irritation = 7 | Inactive = 3200 ms\n");
  assert(lastTelemetryAt == 200);
  assert(telemetryReadCount == 1);
  assert(moodSetCount == 0);

  const BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged,
                            BuddyMood::Grumpy, BuddyMood::Sleepy};
  const char* commands[] = {"mc", "me", "mg", "ms"};
  const char* names[] = {"Calm", "Engaged", "Grumpy", "Sleepy"};
  for (int index = 0; index < 4; ++index) {
    Serial.clearOutput();
    sendCommand(commands[index]);
    assert(currentMood == moods[index]);
    assert(moodSetCount == index + 1);
    assert(lastMoodSetAt == 200);
    const std::string expected = std::string("DIAG: Mood = ") + names[index] + "\n";
    assert(Serial.output == expected);
    Serial.clearOutput();
    sendCommand("m?");
    assert(Serial.output == std::string("DIAG: Mood = ") + names[index] +
        " | Engagement = 45 | Irritation = 7 | Inactive = 3200 ms\n");
    assert(moodSetCount == index + 1);
    assert(lastMoodSetAt == 200);
  }
  assert(triggerCount == triggersBeforeMood);
  Serial.clearOutput();
  const int readsBeforeIdle = telemetryReadCount;
  updateDiagnostics(250);
  assert(Serial.output.empty());
  assert(telemetryReadCount == readsBeforeIdle);

  // A selector can arrive in a later loop, with no mutation while pending.
  Serial.clearOutput();
  Serial.push('m');
  updateDiagnostics(300);
  updateDiagnostics(301);  // No serial input; return immediately.
  assert(currentMood == BuddyMood::Sleepy);
  assert(moodSetCount == 4);
  assert(Serial.output.empty());
  Serial.push('g');
  updateDiagnostics(302);
  assert(currentMood == BuddyMood::Grumpy);
  assert(moodSetCount == 5);
  assert(lastMoodSetAt == 302);
  assert(Serial.output == "DIAG: Mood = Grumpy\n");

  // Invalid selectors are consumed and normal reaction/variant parsing resumes.
  Serial.clearOutput();
  sendCommand("mx1");
  assert(Serial.output.find("DIAG: Invalid mood command") != std::string::npos);
  assert(currentMood == BuddyMood::Grumpy);
  assert(moodSetCount == 5);
  assert(triggerCount == triggersBeforeMood + 1);
  assert(lastReaction == DiagnosticReaction::Happy);
  sendCommand("v11");
  assert(lastVariant == DiagnosticSoundVariant::Variant1);
  coreState = BuddyCoreState::Awake;
  const char* autonomousCommands[] = {"a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8"};
  const char* autonomousNames[] = {"Curious", "Daydreaming", "SideGlance", "Bored",
      "SuspiciousGlance", "AnnoyedSquint", "SleepyDrift", "ExcitedScanning"};
  for (uint8_t index = 0; index < 8; ++index) {
    Serial.clearOutput();
    sendCommand(autonomousCommands[index]);
    assert(lastAutonomous == static_cast<DiagnosticAutonomousBehavior>(index));
    assert(autonomousTriggerCount == index + 1);
    assert(Serial.output == std::string("DIAG: ") + autonomousNames[index] + " / Silent\n");
  }
  Serial.clearOutput();
  sendCommand("a?");
  assert(Serial.output.find("a8: ExcitedScanning") != std::string::npos);
  assert(autonomousTriggerCount == 8);
  Serial.clearOutput();
  Serial.push('a');
  updateDiagnostics(300);
  updateDiagnostics(301);
  assert(Serial.output.empty());
  Serial.push('x');
  updateDiagnostics(302);
  assert(Serial.output.find("Invalid autonomous command") != std::string::npos);
  sendCommand("A3v14");
  assert(autonomousTriggerCount == 9);
  Serial.clearOutput();
  sendCommand("t?");
  assert(Serial.output == "DIAG: Autonomous scheduling disabled in diagnostics\n");
  assert(timingReadAt == 200);
  const int beforeTiming = timingReadCount;
  Serial.clearOutput();
  Serial.push('t');
  updateDiagnostics(300);
  updateDiagnostics(301);
  assert(Serial.output.empty() && timingReadCount == beforeTiming);
  Serial.push('x');
  updateDiagnostics(302);
  assert(Serial.output == "DIAG: Invalid timing command (use t?)\n");
  timing = {true, BuddyMood::Engaged, 17420, 12000, 24000};
  Serial.clearOutput();
  sendCommand("T?");
  assert(Serial.output == "DIAG: Autonomous = Scheduled | Mood = Engaged | Remaining = 17420 ms | Range = 12000-24000 ms\n");
  sendCommand("txm?i?a?");  // Invalid timing selector does not poison other parsers.
  assert(lastAutonomous == DiagnosticAutonomousBehavior::SideGlance);
  assert(lastReaction == DiagnosticReaction::Startled);
  coreState = BuddyCoreState::Sleeping;
  Serial.clearOutput();
  sendCommand("a8");
  assert(Serial.output == "DIAG: Ignored while sleeping\n");
  assert(autonomousTriggerCount == 9);
  sendCommand("v21");
  assert(lastVariant == DiagnosticSoundVariant::Variant2);
  sendCommand("v31");
  assert(lastVariant == DiagnosticSoundVariant::Variant3);
  sendCommand("vr1");
  assert(lastVariant == DiagnosticSoundVariant::Random);

  // Mood reporting and selection work while sleeping without waking Buddy.
  coreState = BuddyCoreState::Sleeping;
  Serial.clearOutput();
  sendCommand("m?");
  assert(Serial.output == "DIAG: Mood = Grumpy | Engagement = 45 | Irritation = 7 | Inactive = 0 ms\n");
  assert(moodSetCount == 5);
  sendCommand("mc");
  sendCommand("mg");
  assert(currentMood == BuddyMood::Grumpy);
  assert(coreState == BuddyCoreState::Sleeping);

  Serial.clearOutput();
  sendCommand("i?");
  assert(Serial.output == "DIAG: Interaction = None\n");
  assert(interactionReadAt == 200);
  const auto readsBeforePending = interactionReadCount;
  Serial.clearOutput();
  Serial.push('i');
  updateDiagnostics(300);
  updateDiagnostics(301);
  assert(Serial.output.empty());
  assert(interactionReadCount == readsBeforePending);
  Serial.push('x');
  updateDiagnostics(302);
  assert(Serial.output == "DIAG: Invalid interaction command (use i?)\n");
  assert(interactionReadCount == readsBeforePending);
  Serial.clearOutput();
  interaction = {DiagnosticRecentInteractionType::Sound, 6200, false};
  sendCommand("I?");
  assert(Serial.output == "DIAG: Interaction = Sound | Age = 6200 ms | Recent = No\n");
  interaction = {DiagnosticRecentInteractionType::TouchTap, 1250, true};
  Serial.clearOutput();
  sendCommand("i \n?");
  assert(Serial.output == "DIAG: Interaction = TouchTap | Age = 1250 ms | Recent = Yes\n");
  interaction = {DiagnosticRecentInteractionType::TouchHold, 0, true};
  Serial.clearOutput();
  sendCommand("i?");
  assert(Serial.output == "DIAG: Interaction = TouchHold | Age = 0 ms | Recent = Yes\n");
  const int triggersBeforeRecovery = triggerCount;
  sendCommand("ixv14m?");
  assert(triggerCount == triggersBeforeRecovery + 1);
  assert(lastReaction == DiagnosticReaction::Startled);
  assert(lastVariant == DiagnosticSoundVariant::Variant1);
  Serial.clearOutput(); sendCommand("q?");
  assert(Serial.output == "DIAG: Sound Mode = Normal\n");
  Serial.clearOutput(); Serial.push('q'); updateDiagnostics(300); updateDiagnostics(301);
  assert(Serial.output.empty());
  sendCommand("x"); assert(Serial.output == "DIAG: Invalid quiet command (use q?)\n");
  Serial.clearOutput(); sendCommand("Q \n?");
  assert(Serial.output == "DIAG: Sound Mode = Normal\n");
  return 0;
}

DiagnosticSoundSelection getDiagnosticSoundSelection() {
  return {false, ReactionSound::None, BuddyMood::Calm, 0};
}
DiagnosticSoundWeights getDiagnosticSoundWeights(ReactionSound, BuddyMood) { return {{1, 1, 1}}; }

BuddySoundMode getBuddySoundMode() { return BuddySoundMode::Normal; }

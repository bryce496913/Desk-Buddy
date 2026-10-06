#include <cassert>
#include <cstdint>
#include <string>

#include "Arduino.h"
#include "Diagnostics.h"
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
BuddyCoreState coreState = BuddyCoreState::Awake;

void sendCommand(const char* command) {
  for (const char* cursor = command; *cursor; ++cursor) {
    Serial.push(*cursor);
    updateDiagnostics(200);
  }
}
}

BuddyCoreState getBuddyCoreState() { return coreState; }
BuddyMood getBuddyMood() { return currentMood; }
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
  return 0;
}

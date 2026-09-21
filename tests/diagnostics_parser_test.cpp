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
}

BuddyCoreState getBuddyCoreState() { return BuddyCoreState::Awake; }

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
  return 0;
}

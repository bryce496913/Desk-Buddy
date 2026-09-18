#include "Diagnostics.h"

#if DESK_BUDDY_DIAGNOSTICS

#include "BehaviorEngine.h"

namespace {
DiagnosticSoundVariant soundVariant = DiagnosticSoundVariant::Random;
bool variantSelectorPending = false;

const char* variantName(DiagnosticSoundVariant variant) {
  switch (variant) {
    case DiagnosticSoundVariant::Random: return "Random";
    case DiagnosticSoundVariant::Variant1: return "Variant 1";
    case DiagnosticSoundVariant::Variant2: return "Variant 2";
    case DiagnosticSoundVariant::Variant3: return "Variant 3";
  }
  return "Unknown";
}

void printDiagnosticHelp() {
  Serial.println("Desk Buddy diagnostics");
  Serial.println();
  Serial.println("Reactions:");
  Serial.println("1: Happy");
  Serial.println("2: Curious");
  Serial.println("3: Annoyed");
  Serial.println("4: Startled");
  Serial.println("5: Suspicious");
  Serial.println("6: Confused");
  Serial.println("7: Daydreaming");
  Serial.println("0: Normal");
  Serial.println();
  Serial.println("Sound variant:");
  Serial.println("v1: Variant 1");
  Serial.println("v2: Variant 2");
  Serial.println("v3: Variant 3");
  Serial.println("vr: Random");
  Serial.print("Sound variant mode: ");
  Serial.println(variantName(soundVariant));
  Serial.println();
  Serial.println("?: Help");
}

const char* reactionName(DiagnosticReaction reaction) {
  switch (reaction) {
    case DiagnosticReaction::Normal: return "Normal";
    case DiagnosticReaction::Happy: return "Happy";
    case DiagnosticReaction::Curious: return "Curious";
    case DiagnosticReaction::Annoyed: return "Annoyed";
    case DiagnosticReaction::Startled: return "Startled";
    case DiagnosticReaction::Suspicious: return "Suspicious";
    case DiagnosticReaction::Confused: return "Confused";
    case DiagnosticReaction::Daydreaming: return "Daydreaming";
  }
  return "Unknown";
}
}  // namespace

void beginDiagnostics() { printDiagnosticHelp(); }

void updateDiagnostics(uint32_t now) {
  if (Serial.available() <= 0) return;

  const char command = static_cast<char>(Serial.read());
  if (command == '\r' || command == '\n' || command == ' ') return;

  if (variantSelectorPending) {
    variantSelectorPending = false;
    switch (command) {
      case '1': soundVariant = DiagnosticSoundVariant::Variant1; break;
      case '2': soundVariant = DiagnosticSoundVariant::Variant2; break;
      case '3': soundVariant = DiagnosticSoundVariant::Variant3; break;
      case 'r': case 'R': soundVariant = DiagnosticSoundVariant::Random; break;
      default:
        Serial.println("DIAG: Invalid sound variant (use v1, v2, v3, or vr)");
        return;
    }
    Serial.print("DIAG: Sound variant = ");
    Serial.println(variantName(soundVariant));
    return;
  }

  if (command == 'v' || command == 'V') {
    variantSelectorPending = true;
    return;
  }
  if (command == '?') {
    printDiagnosticHelp();
    return;
  }
  if (command < '0' || command > '7') {
    Serial.println("DIAG: Unknown command (?: Help)");
    return;
  }

  const DiagnosticReaction reaction =
      static_cast<DiagnosticReaction>(command - '0');
  uint8_t selectedVariantIndex = DIAGNOSTIC_RANDOM_VARIANT;
  if (!triggerDiagnosticReaction(reaction, soundVariant, now,
                                 selectedVariantIndex)) {
    if (getBuddyCoreState() == BuddyCoreState::Sleeping) {
      Serial.println("DIAG: Ignored while sleeping");
    } else {
      Serial.println("DIAG: Invalid sound variant");
    }
    return;
  }

  Serial.print("DIAG: ");
  Serial.print(reactionName(reaction));
  if (reaction == DiagnosticReaction::Daydreaming) {
    Serial.println(" / Silent");
  } else if (selectedVariantIndex != DIAGNOSTIC_RANDOM_VARIANT) {
    Serial.print(" / Variant ");
    Serial.println(selectedVariantIndex + 1);
  } else {
    Serial.println();
  }
}

#endif

#include "Diagnostics.h"

#if DESK_BUDDY_DIAGNOSTICS

#include "BehaviorEngine.h"

namespace {
void printDiagnosticHelp() {
  Serial.println("Desk Buddy diagnostics enabled");
  Serial.println("1: Happy");
  Serial.println("2: Curious");
  Serial.println("3: Annoyed");
  Serial.println("4: Startled");
  Serial.println("5: Suspicious");
  Serial.println("6: Confused");
  Serial.println("7: Daydreaming");
  Serial.println("0: Normal");
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
  if (!triggerDiagnosticReaction(reaction, now)) {
    Serial.println("DIAG: Ignored while sleeping");
    return;
  }

  Serial.print("DIAG: ");
  Serial.println(reactionName(reaction));
}

#endif

#include "Diagnostics.h"

#if DESK_BUDDY_DIAGNOSTICS

#include "BehaviorEngine.h"
#include "SoundEngine.h"

namespace {
DiagnosticSoundVariant soundVariant = DiagnosticSoundVariant::Random;
bool variantSelectorPending = false;
bool moodSelectorPending = false;
bool interactionSelectorPending = false;
bool autonomousSelectorPending = false;
bool timingSelectorPending = false;
bool soundSelectorPending = false;
bool quietSelectorPending = false;
bool eventSelectorPending = false;
const char* const autonomousNames[] = {
    "Curious", "Daydreaming", "SideGlance", "Bored", "SuspiciousGlance",
    "AnnoyedSquint", "SleepyDrift", "ExcitedScanning"};

void printAutonomousHelp() {
  Serial.println("Autonomous showcase (silent):");
  for (uint8_t index = 0; index < 8; ++index) {
    Serial.print("a");
    Serial.print(static_cast<unsigned int>(index + 1));
    Serial.print(": ");
    Serial.println(autonomousNames[index]);
  }
  Serial.println("a?: Autonomous showcase help");
}

const char* interactionName(DiagnosticRecentInteractionType type) {
  switch (type) {
    case DiagnosticRecentInteractionType::None: return "None";
    case DiagnosticRecentInteractionType::TouchTap: return "TouchTap";
    case DiagnosticRecentInteractionType::TouchHold: return "TouchHold";
    case DiagnosticRecentInteractionType::Sound: return "Sound";
  }
  return "Unknown";
}

void printInteractionTelemetry(uint32_t now) {
  const auto context = getDiagnosticRecentInteractionContext(now);
  Serial.print("DIAG: Interaction = ");
  Serial.print(interactionName(context.type));
  if (context.type != DiagnosticRecentInteractionType::None) {
    Serial.print(" | Age = ");
    Serial.print(context.ageMs);
    Serial.print(" ms | Recent = ");
    Serial.print(context.recent ? "Yes" : "No");
  }
  Serial.println();
}

const char* moodName(BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Calm: return "Calm";
    case BuddyMood::Engaged: return "Engaged";
    case BuddyMood::Grumpy: return "Grumpy";
    case BuddyMood::Sleepy: return "Sleepy";
  }
  return "Unknown";
}

void printCurrentMood() {
  Serial.print("DIAG: Mood = ");
  Serial.println(moodName(getBuddyMood()));
}

void printTimingTelemetry(uint32_t now) {
  const auto state = getDiagnosticAutonomousTimingState(now);
  if (!state.scheduled) {
    Serial.println("DIAG: Autonomous scheduling disabled in diagnostics");
    return;
  }
  Serial.print("DIAG: Autonomous = Scheduled | Mood = ");
  Serial.print(moodName(state.scheduleMood));
  Serial.print(" | Remaining = ");
  Serial.print(state.remainingMs);
  Serial.print(" ms | Range = ");
  Serial.print(state.minMs);
  Serial.print("-");
  Serial.print(state.maxMs);
  Serial.println(" ms");
}

void printMoodTelemetry(uint32_t now) {
  const DiagnosticMoodState state = getDiagnosticMoodState(now);
  Serial.print("DIAG: Mood = ");
  Serial.print(moodName(state.mood));
  Serial.print(" | Engagement = ");
  Serial.print(static_cast<unsigned int>(state.engagementScore));
  Serial.print(" | Irritation = ");
  Serial.print(static_cast<unsigned int>(state.irritationScore));
  Serial.print(" | Inactive = ");
  Serial.print(state.inactivityMs);
  Serial.println(" ms");
}

const char* variantName(DiagnosticSoundVariant variant) {
  switch (variant) {
    case DiagnosticSoundVariant::Random: return "Random";
    case DiagnosticSoundVariant::Variant1: return "Variant 1";
    case DiagnosticSoundVariant::Variant2: return "Variant 2";
    case DiagnosticSoundVariant::Variant3: return "Variant 3";
  }
  return "Unknown";
}

const char* soundName(ReactionSound sound) {
  switch (sound) {
    case ReactionSound::Happy: return "Happy";
    case ReactionSound::Curious: return "Curious";
    case ReactionSound::Annoyed: return "Annoyed";
    case ReactionSound::Startled: return "Startled";
    case ReactionSound::Suspicious: return "Suspicious";
    case ReactionSound::Confused: return "Confused";
    default: return "None";
  }
}
void printSoundSelection() {
  const auto selection = getDiagnosticSoundSelection();
  Serial.print("DIAG: Sound = ");
  if (!selection.valid) { Serial.println("None"); return; }
  Serial.print(soundName(selection.sound));
  Serial.print(" | Mood = "); Serial.print(moodName(selection.mood));
  Serial.print(" | Variant = ");
  Serial.println(static_cast<unsigned int>(selection.variantIndex + 1));
}
void printSoundWeights() {
  const BuddyMood mood = getBuddyMood();
  Serial.print("DIAG SOUND WEIGHTS - "); Serial.println(moodName(mood));
  const ReactionSound families[] = {ReactionSound::Happy, ReactionSound::Curious,
       ReactionSound::Annoyed, ReactionSound::Startled, ReactionSound::Suspicious,
       ReactionSound::Confused};
  for (ReactionSound sound : families) {
    const auto weights = getDiagnosticSoundWeights(sound, mood);
    Serial.print(soundName(sound)); Serial.print(" ");
    Serial.print(static_cast<unsigned int>(weights.values[0])); Serial.print(" / ");
    Serial.print(static_cast<unsigned int>(weights.values[1])); Serial.print(" / ");
    Serial.println(static_cast<unsigned int>(weights.values[2]));
  }
}

void printEventHelp() {
  Serial.println("Diagnostic production events (mutate real interaction state):");
  Serial.println("et: Simulate TouchTap");
  Serial.println("eh: Simulate TouchHold");
  Serial.println("ea: One production mood-weighted autonomous event (unlike a1-a8 showcases)");
}

void printDiagnosticHelp() {
  Serial.println("e?: Production event help | et: TouchTap | eh: TouchHold | ea: Production autonomy");
  Serial.println("q?: Sound mode | qn: Normal | qq: Quiet");
  Serial.println("s?: Last random sound selection | sw: Current mood sound weights");
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
  Serial.println();
  Serial.println("Mood:");
  Serial.println("m?: Current mood, scores, waking inactivity (0 while sleeping)");
  Serial.println("mc: Calm");
  Serial.println("me: Engaged");
  Serial.println("mg: Grumpy");
  Serial.println("ms: Sleepy");
  Serial.println();
  Serial.println("i?: Recent interaction type, age, and validity");
  Serial.println("t?: Autonomous timing (background scheduling disabled in diagnostics)");
  Serial.println();
  printAutonomousHelp();
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

  if (eventSelectorPending) {
    eventSelectorPending = false;
    if (command == '?') { printEventHelp(); return; }
    if (command == 'a' || command == 'A') {
      DiagnosticAutonomousBehavior selected;
      if (!triggerDiagnosticAutonomousEvent(now, selected)) {
        Serial.println("DIAG: Autonomous event ignored while sleeping");
      } else {
        Serial.print("DIAG AUTO: Mood = "); Serial.print(moodName(getBuddyMood()));
        Serial.print(" | Selected = ");
        Serial.println(autonomousNames[static_cast<uint8_t>(selected)]);
      }
      return;
    }
    const bool tap = command == 't' || command == 'T';
    const bool hold = command == 'h' || command == 'H';
    if (!tap && !hold) {
      Serial.println("DIAG: Invalid event command (use e?, et, eh or ea)");
      return;
    }
    // Production events intentionally mutate history and obey Quiet/Sleep,
    // unlike direct reaction showcases and explicit sound auditions.
    processBuddyEvent(tap ? BuddyEvent::TouchTap : BuddyEvent::TouchHold, now);
    if (getBuddyCoreState() == BuddyCoreState::Sleeping) {
      Serial.println("DIAG: Event ignored while sleeping");
    } else Serial.println(tap ? "DIAG EVENT: TouchTap" : "DIAG EVENT: TouchHold");
    return;
  }

  if (quietSelectorPending) {
    quietSelectorPending = false;
    if (command == '?' || command == 'n' || command == 'N' || command == 'q' || command == 'Q') {
      if (command == 'n' || command == 'N') setDiagnosticSoundMode(BuddySoundMode::Normal);
      if (command == 'q' || command == 'Q') setDiagnosticSoundMode(BuddySoundMode::Quiet);
      Serial.print("DIAG: Sound Mode = ");
      Serial.println(getBuddySoundMode() == BuddySoundMode::Quiet ? "Quiet" : "Normal");
    } else Serial.println("DIAG: Invalid quiet command (use q?, qn or qq)");
    return;
  }

  if (soundSelectorPending) {
    soundSelectorPending = false;
    if (command == '?') printSoundSelection();
    else if (command == 'w' || command == 'W') printSoundWeights();
    else Serial.println("DIAG: Invalid sound command (use s? or sw)");
    return;
  }

  if (timingSelectorPending) {
    timingSelectorPending = false;
    if (command == '?') printTimingTelemetry(now);
    else Serial.println("DIAG: Invalid timing command (use t?)");
    return;
  }

  if (autonomousSelectorPending) {
    autonomousSelectorPending = false;
    if (command == '?') {
      printAutonomousHelp();
    } else if (command >= '1' && command <= '8') {
      const uint8_t index = static_cast<uint8_t>(command - '1');
      if (triggerDiagnosticAutonomousBehavior(
              static_cast<DiagnosticAutonomousBehavior>(index), now)) {
        Serial.print("DIAG: ");
        Serial.print(autonomousNames[index]);
        Serial.println(" / Silent");
      } else {
        Serial.println("DIAG: Ignored while sleeping");
      }
    } else {
      Serial.println("DIAG: Invalid autonomous command (use a1-a8 or a?)");
    }
    return;
  }

  if (interactionSelectorPending) {
    interactionSelectorPending = false;
    if (command == '?') {
      printInteractionTelemetry(now);
    } else {
      Serial.println("DIAG: Invalid interaction command (use i?)");
    }
    return;
  }

  if (moodSelectorPending) {
    moodSelectorPending = false;
    switch (command) {
      case '?': printMoodTelemetry(now); return;
      case 'c': setDiagnosticMood(BuddyMood::Calm, now); break;
      case 'e': setDiagnosticMood(BuddyMood::Engaged, now); break;
      case 'g': setDiagnosticMood(BuddyMood::Grumpy, now); break;
      case 's': setDiagnosticMood(BuddyMood::Sleepy, now); break;
      default:
        Serial.println("DIAG: Invalid mood command (use m?, mc, me, mg, or ms)");
        return;
    }
    printCurrentMood();
    return;
  }

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
  if (command == 'm' || command == 'M') {
    moodSelectorPending = true;
    return;
  }
  if (command == 'i' || command == 'I') {
    interactionSelectorPending = true;
    return;
  }
  if (command == 'a' || command == 'A') {
    autonomousSelectorPending = true;
    return;
  }
  if (command == 'e' || command == 'E') {
    eventSelectorPending = true;
    return;
  }
  if (command == 'q' || command == 'Q') {
    quietSelectorPending = true;
    return;
  }
  if (command == 's' || command == 'S') {
    soundSelectorPending = true;
    return;
  }
  if (command == 't' || command == 'T') {
    timingSelectorPending = true;
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

#include <cassert>
#include <cstdint>
#include <limits>
#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"
#include "Diagnostics.h"

static_assert(DESK_BUDDY_DIAGNOSTICS == 1, "cross context tests require diagnostics");
HardwareSerial Serial;
namespace {
FaceExpression expression = FaceExpression::Normal;
ReactionSound sound = ReactionSound::None;
bool faceFinished = false;
bool soundActive = false;
int faceStarts = 0;
int soundStarts = 0;

void beginAt(uint32_t now = 0) {
  expression = FaceExpression::Normal;
  sound = ReactionSound::None;
  faceFinished = false;
  soundActive = false;
  faceStarts = 0;
  soundStarts = 0;
  beginBehaviorEngine(now);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}

void expectMood(uint32_t now, BuddyMood mood, uint8_t engagement,
                uint8_t irritation, uint32_t inactivity) {
  const DiagnosticMoodState state = getDiagnosticMoodState(now);
  assert(state.mood == mood);
  assert(getBuddyMood() == mood);
  assert(state.engagementScore == engagement);
  assert(state.irritationScore == irritation);
  assert(state.inactivityMs == inactivity);
}

void expectReaction(FaceExpression expectedExpression,
                    ReactionSound expectedSound) {
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(expression == expectedExpression);
  assert(sound == expectedSound);
}

void finishAt(uint32_t now) {
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(now);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}


using Type = DiagnosticRecentInteractionType;
void memory(uint32_t now, Type type, uint32_t age = 0, bool recent = true) {
  const auto state = getDiagnosticRecentInteractionContext(now);
  assert(state.type == type && state.ageMs == age && state.recent == recent);
}
void command(const char* text, uint32_t now) {
  for (const char* cursor = text; *cursor; ++cursor) {
    Serial.push(*cursor);
    updateDiagnostics(now);
  }
}
void testIsolatedAndCrossDirections() {
  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  memory(1000, Type::Sound);

  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1001);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
  processBuddyEvent(BuddyEvent::SoundDetected, 1002);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.

  beginAt();
  processBuddyEvent(BuddyEvent::TouchHold, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  processBuddyEvent(BuddyEvent::TouchTap, 1001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(1001, BuddyMood::Calm, 25, 0, 0);

  beginAt();
  setDiagnosticMood(BuddyMood::Grumpy, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  processBuddyEvent(BuddyEvent::TouchHold, 1001);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(1001, BuddyMood::Calm, 35, 45, 0);
}
void testOneStepChainAndHistories() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  memory(1000, Type::TouchTap);
  processBuddyEvent(BuddyEvent::SoundDetected, 1001);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
  memory(1001, Type::Sound);
  processBuddyEvent(BuddyEvent::TouchHold, 1002);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  memory(1002, Type::TouchHold);
  setDiagnosticMood(BuddyMood::Engaged, 1002);
  processBuddyEvent(BuddyEvent::TouchTap, 1003);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);  // Tap #2, Hold context.
  expectMood(1003, BuddyMood::Engaged, 65, 0, 0);
  memory(1003, Type::TouchTap);

  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1001);
  setDiagnosticMood(BuddyMood::Calm, 1001);
  processBuddyEvent(BuddyEvent::TouchTap, 1002);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(1002, BuddyMood::Calm, 25, 0, 0);  // Tap #2, not #1 or #3.

  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  processBuddyEvent(BuddyEvent::TouchHold, 1001);
  setDiagnosticMood(BuddyMood::Grumpy, 1001);
  processBuddyEvent(BuddyEvent::SoundDetected, 1002);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Hold-context Sound #2.
  expectMood(1002, BuddyMood::Grumpy, 5, 60, 0);
}
void testBoundariesAndRollover() {
  const uint32_t anchors[] = {1000, std::numeric_limits<uint32_t>::max() - 1000};
  for (uint32_t anchor : anchors) {
    for (uint32_t age : {5000u, 5001u}) {
      const bool recent = age == 5000;
      for (BuddyEvent touch : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
        beginAt(anchor);
        processBuddyEvent(touch, anchor);
        memory(anchor + age, touch == BuddyEvent::TouchTap ? Type::TouchTap : Type::TouchHold,
               age, recent);
        processBuddyEvent(BuddyEvent::SoundDetected, anchor + age);
        if (!recent) expectReaction(FaceExpression::Startled, ReactionSound::Startled);
        else if (touch == BuddyEvent::TouchTap) expectReaction(FaceExpression::Confused, ReactionSound::Confused);
        else expectReaction(FaceExpression::Curious, ReactionSound::Curious);
        memory(anchor + age, Type::Sound);
      }
      for (BuddyEvent touch : {BuddyEvent::TouchTap, BuddyEvent::TouchHold}) {
        beginAt(anchor);
        processBuddyEvent(BuddyEvent::SoundDetected, anchor);
        setDiagnosticMood(touch == BuddyEvent::TouchHold ? BuddyMood::Grumpy : BuddyMood::Calm, anchor);
        memory(anchor + age, Type::Sound, age, recent);
        processBuddyEvent(touch, anchor + age);
        const bool happy = touch == BuddyEvent::TouchHold ? recent : !recent;
        expectReaction(happy ? FaceExpression::Happy : FaceExpression::Curious,
                       happy ? ReactionSound::Happy : ReactionSound::Curious);
      }
    }
  }
}
void testSleepAutonomyAndDiagnosticExclusion() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  processBuddyEvent(BuddyEvent::ButtonPressed, 1001);
  memory(1001, Type::None, 0, false);
  Serial.clearOutput();
  command("i?", 1001);
  assert(Serial.output == "DIAG: Interaction = None\n");
  for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold, BuddyEvent::SoundDetected}) {
    processBuddyEvent(event, 1002);
    memory(1002, Type::None, 0, false);
  }
  processBuddyEvent(BuddyEvent::ButtonPressed, 1003);
  memory(1003, Type::None, 0, false);
  Serial.clearOutput();
  command("i?", 1003);
  assert(Serial.output == "DIAG: Interaction = None\n");
  processBuddyEvent(BuddyEvent::SoundDetected, 1004);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);

  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  finishAt(1001);
  processBuddyEvent(BuddyEvent::IdleTimeout, 1002);
  memory(1002, Type::TouchTap, 2);
  processBuddyEvent(BuddyEvent::SoundDetected, 1003);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);

  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  command("4mc", 1001);  // Direct reaction and mood commands preserve memory.
  memory(1001, Type::TouchTap, 1);
  processBuddyEvent(BuddyEvent::SoundDetected, 1002);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
}
void testMoodCoexistence() {
  beginAt();
  setDiagnosticMood(BuddyMood::Engaged, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  processBuddyEvent(BuddyEvent::TouchTap, 1001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(1001, BuddyMood::Engaged, 65, 0, 0);

  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 1000);
  setDiagnosticMood(BuddyMood::Sleepy, 1001);
  processBuddyEvent(BuddyEvent::TouchHold, 1002);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(1002, BuddyMood::Calm, 30, 0, 0);
}
void testTelemetryIsReadOnly() {
  beginAt();
  Serial.clearOutput();
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  processBuddyEvent(BuddyEvent::SoundDetected, 1001);
  assert(Serial.output.empty());  // No continuous interaction logging.
  const auto before = getDiagnosticMoodState(1001);
  const int facesBefore = faceStarts;
  const int soundsBefore = soundStarts;
  command("t?", 2251);
  assert(Serial.output == "DIAG: Autonomous scheduling disabled in diagnostics\n");
  memory(2251, Type::Sound, 1250);
  Serial.clearOutput();
  command("i?", 2251);
  assert(Serial.output == "DIAG: Interaction = Sound | Age = 1250 ms | Recent = Yes\n");
  Serial.clearOutput();
  command("i?", 7201);
  assert(Serial.output == "DIAG: Interaction = Sound | Age = 6200 ms | Recent = No\n");
  memory(6001, Type::Sound, 5000);
  memory(6002, Type::Sound, 5001, false);
  const auto after = getDiagnosticMoodState(1001);
  assert(before.mood == after.mood && before.engagementScore == after.engagementScore);
  assert(before.irritationScore == after.irritationScore && before.inactivityMs == after.inactivityMs);
  assert(faceStarts == facesBefore && soundStarts == soundsBefore);
  // Back at a deterministic event timestamp: telemetry did not alter either streak.
  processBuddyEvent(BuddyEvent::TouchTap, 1002);
  expectMood(1002, BuddyMood::Engaged, 50, 0, 0);  // Tap #2.
  processBuddyEvent(BuddyEvent::SoundDetected, 1003);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.
  expectMood(1003, BuddyMood::Engaged, 55, 0, 0);
}
}  // namespace

long random(long) { return 0; }
long random(long minimum, long) { return minimum; }
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t, FaceExpression requested) {
  expression = requested;
  faceFinished = false;
  ++faceStarts;
}
bool isFaceReactionFinished(uint32_t) { return faceFinished; }
void finishFaceReaction(uint32_t) {
  expression = FaceExpression::Normal;
  faceFinished = false;
}
void enterSleepFace(uint32_t) {}
void wakeFace(uint32_t) {}
void startReactionSound(uint32_t, ReactionSound requested) {
  sound = requested;
  soundActive = sound != ReactionSound::None;
  ++soundStarts;
}
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requested,
                                  uint8_t, uint8_t &selected) {
  selected = requested == ReactionSound::None ? DIAGNOSTIC_RANDOM_VARIANT : 0;
  startReactionSound(now, requested);
  return true;
}
void stopReactionSound() {
  sound = ReactionSound::None;
  soundActive = false;
}
bool isSoundEngineActive() { return soundActive; }
void playSleepSound() {}
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}

int main() {
  testIsolatedAndCrossDirections();
  testOneStepChainAndHistories();
  testBoundariesAndRollover();
  testSleepAutonomyAndDiagnosticExclusion();
  testMoodCoexistence();
  testTelemetryIsReadOnly();
}

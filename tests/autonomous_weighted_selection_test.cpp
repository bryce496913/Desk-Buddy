#include <cassert>
#include <cstdint>
#include "autonomous_test_helpers.h"
#include "SoundEngine.h"
#include "../BehaviorEngine.cpp"

namespace {
long requestedTicket = 0;
long observedTotal = 0;
int randomCalls = 0;
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


struct ExpectedCandidate { AutonomousBehavior behavior; uint8_t weight; };
constexpr ExpectedCandidate expectedPools[][4] = {
    {{AutonomousBehavior::Daydreaming,30},{AutonomousBehavior::SideGlance,25},
     {AutonomousBehavior::Curious,25},{AutonomousBehavior::Bored,20}},
    {{AutonomousBehavior::ExcitedScanning,35},{AutonomousBehavior::Curious,30},
     {AutonomousBehavior::SideGlance,20},{AutonomousBehavior::Daydreaming,15}},
    {{AutonomousBehavior::AnnoyedSquint,40},{AutonomousBehavior::SuspiciousGlance,35},
     {AutonomousBehavior::SideGlance,15},{AutonomousBehavior::Bored,10}},
    {{AutonomousBehavior::SleepyDrift,45},{AutonomousBehavior::Daydreaming,30},
     {AutonomousBehavior::Bored,20},{AutonomousBehavior::SideGlance,5}}};
constexpr BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy};

void draw(BuddyMood mood, long ticket, long total, AutonomousBehavior expected,
          bool previousExists = false, AutonomousBehavior previous = AutonomousBehavior::Curious) {
  hasLastAutonomousBehavior = previousExists;
  lastAutonomousBehavior = previous;
  requestedTicket = ticket;
  const int callsBefore = randomCalls;
  const auto selected = selectAutonomousBehavior(mood);
  assert(selected == expected);
  assert(observedTotal == total && randomCalls == callsBefore + 1);
  assert(lastAutonomousBehavior == selected && hasLastAutonomousBehavior);
  const auto plan = planForAutonomousBehavior(selected);
  assert(plan.sound == ReactionSound::None);
  assert(isAutonomousExpressionForMood(plan.expression, mood));
}
void testExactWeightsAndExclusions() {
  for (uint8_t mood = 0; mood < 4; ++mood) {
    long lower = 0;
    for (const auto candidate : expectedPools[mood]) {
      // Every ticket verifies membership and both ends of every range.
      for (long ticket = lower; ticket < lower + candidate.weight; ++ticket) {
        draw(moods[mood], ticket, 100, candidate.behavior);
      }
      lower += candidate.weight;
    }
    assert(lower == 100);
    for (const auto excluded : expectedPools[mood]) {
      lower = 0;
      for (const auto candidate : expectedPools[mood]) {
        if (candidate.behavior == excluded.behavior) continue;
        for (long ticket = lower; ticket < lower + candidate.weight; ++ticket) {
          draw(moods[mood], ticket, 100 - excluded.weight, candidate.behavior, true, excluded.behavior);
        }
        lower += candidate.weight;
      }
      assert(lower == 100 - excluded.weight);
    }
  }
}
void testCrossMoodExclusion() {
  draw(BuddyMood::Calm, 30, 100, AutonomousBehavior::SideGlance);
  draw(BuddyMood::Engaged, 34, 80, AutonomousBehavior::ExcitedScanning, true, AutonomousBehavior::SideGlance);
  draw(BuddyMood::Engaged, 35, 80, AutonomousBehavior::Curious, true, AutonomousBehavior::SideGlance);
  draw(BuddyMood::Engaged, 64, 80, AutonomousBehavior::Curious, true, AutonomousBehavior::SideGlance);
  draw(BuddyMood::Engaged, 65, 80, AutonomousBehavior::Daydreaming, true, AutonomousBehavior::SideGlance);
  draw(BuddyMood::Engaged, 79, 80, AutonomousBehavior::Daydreaming, true, AutonomousBehavior::SideGlance);
  // Previous behavior absent from new pool: full weights remain available.
  draw(BuddyMood::Grumpy, 0, 100, AutonomousBehavior::AnnoyedSquint, true, AutonomousBehavior::ExcitedScanning);
  draw(BuddyMood::Grumpy, 99, 100, AutonomousBehavior::Bored, true, AutonomousBehavior::ExcitedScanning);
}
void beginSelectionAt(uint32_t now = 0) {
  requestedTicket = 0;
  hasLastAutonomousBehavior = false;
  beginAt(now);
}
void testNaturalMoodTransitionsAndPreemption() {
  beginSelectionAt();
  processBuddyEvent(BuddyEvent::IdleTimeout, 100);
  expectReaction(FaceExpression::Daydreaming, ReactionSound::None);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  processBuddyEvent(BuddyEvent::TouchTap, 102);
  finishAt(103);
  processBuddyEvent(BuddyEvent::IdleTimeout, 104);
  expectReaction(FaceExpression::ExcitedScanning, ReactionSound::None);
  expectMood(104, BuddyMood::Engaged, 45, 0, 2);
  for (uint32_t now = 105; now <= 107; ++now) processBuddyEvent(BuddyEvent::TouchTap, now);
  finishAt(108);
  processBuddyEvent(BuddyEvent::IdleTimeout, 109);
  expectReaction(FaceExpression::AnnoyedSquint, ReactionSound::None);
  expectMood(109, BuddyMood::Grumpy, 60, 75, 2);
  finishAt(110);
  updateBehaviorEngine(100000);
  expectMood(100000, BuddyMood::Sleepy, 10, 0, 99893);
  processBuddyEvent(BuddyEvent::IdleTimeout, 100000);
  expectReaction(FaceExpression::SleepyDrift, ReactionSound::None);
  processBuddyEvent(BuddyEvent::TouchTap, 100001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(100001, BuddyMood::Calm, 30, 0, 0);

  beginSelectionAt();
  processBuddyEvent(BuddyEvent::IdleTimeout, 90000);
  expectReaction(FaceExpression::SleepyDrift, ReactionSound::None);
  processBuddyEvent(BuddyEvent::SoundDetected, 90001);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  expectMood(90001, BuddyMood::Calm, 5, 0, 0);
}
void testNewAutonomyStateIsolation() {
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy}) {
    beginSelectionAt();
    processBuddyEvent(BuddyEvent::TouchTap, 100);
    processBuddyEvent(BuddyEvent::SoundDetected, 101);
    finishAt(102);
    setDiagnosticMood(mood, 103);
    const auto before = getDiagnosticMoodState(104);
    const auto previous = getDiagnosticRecentInteractionContext(104);
    processBuddyEvent(BuddyEvent::IdleTimeout, 104);
    const auto after = getDiagnosticMoodState(104);
    const auto context = getDiagnosticRecentInteractionContext(104);
    assert(before.mood == after.mood && before.engagementScore == after.engagementScore);
    assert(before.irritationScore == after.irritationScore && before.inactivityMs == after.inactivityMs);
    assert(previous.type == context.type && previous.ageMs == context.ageMs && previous.recent == context.recent);
    assert(sound == ReactionSound::None);
    finishAt(105);
    setDiagnosticMood(BuddyMood::Calm, 106);
    processBuddyEvent(BuddyEvent::TouchTap, 107);
    expectMood(107, BuddyMood::Calm, 25, 0, 0);  // Tap #2.
    processBuddyEvent(BuddyEvent::SoundDetected, 108);
    expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.
  }
  beginSelectionAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  finishAt(101);
  requestedTicket = 30;  // Calm SideGlance.
  processBuddyEvent(BuddyEvent::IdleTimeout, 102);
  expectReaction(FaceExpression::SideGlance, ReactionSound::None);
  processBuddyEvent(BuddyEvent::SoundDetected, 103);
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);  // Still sees Tap.
}
}  // namespace

long random(long maximum) {
  ++randomCalls;
  observedTotal = maximum;
  assert(requestedTicket >= 0 && requestedTicket < maximum);
  return requestedTicket;
}
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
void startReactionSound(uint32_t, ReactionSound requested, BuddyMood) {
  sound = requested;
  soundActive = sound != ReactionSound::None;
  ++soundStarts;
}
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requested,
                                  uint8_t, uint8_t &selected, BuddyMood mood) {
  selected = requested == ReactionSound::None ? DIAGNOSTIC_RANDOM_VARIANT : 0;
  startReactionSound(now, requested, mood);
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
  testExactWeightsAndExclusions();
  testCrossMoodExclusion();
  testNaturalMoodTransitionsAndPreemption();
  testNewAutonomyStateIsolation();
}

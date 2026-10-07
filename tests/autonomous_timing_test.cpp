#include <cassert>
#include <cstdint>
#include <limits>
#include "autonomous_test_helpers.h"
#include "SoundEngine.h"
#include "../BehaviorEngine.cpp"

namespace {
enum class TimingTicket { Minimum, Maximum, Middle };
TimingTicket timingTicket = TimingTicket::Minimum;
long observedMin = 0;
long observedExclusiveMax = 0;
int timingCalls = 0;
FaceExpression expression = FaceExpression::Normal;
ReactionSound sound = ReactionSound::None;
bool faceFinished = false;
bool soundActive = false;
int faceStarts = 0;
int soundStarts = 0;

[[maybe_unused]] void beginAt(uint32_t now = 0) {
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

[[maybe_unused]] void expectReaction(FaceExpression expectedExpression,
                    ReactionSound expectedSound) {
  assert(getBuddyReaction() == BuddyReaction::Generic);
  assert(expression == expectedExpression);
  assert(sound == expectedSound);
}

[[maybe_unused]] void finishAt(uint32_t now) {
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(now);
  assert(getBuddyReaction() == BuddyReaction::Idle);
}



void seedMood(BuddyMood mood, uint32_t now) {
  currentMood = mood;
  engagementScore = mood == BuddyMood::Engaged ? 40 : 0;
  irritationScore = mood == BuddyMood::Grumpy ? 75 : 0;
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = mood == BuddyMood::Sleepy ? now - uint32_t{90000} : now;
}
void testRanges() {
  const BuddyMood moods[] = {BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Calm, BuddyMood::Sleepy};
  const uint32_t minimums[] = {12000, 20000, 28000, 45000};
  const uint32_t maximums[] = {24000, 35000, 45000, 70000};
  for (uint8_t index = 0; index < 4; ++index) {
    const auto range = autonomousTimingFor(moods[index]);
    assert(range.minMs == minimums[index] && range.maxMs == maximums[index]);
    if (index) assert(minimums[index - 1] < minimums[index]);
    for (TimingTicket ticket : {TimingTicket::Minimum, TimingTicket::Maximum, TimingTicket::Middle}) {
      timingTicket = ticket;
      constexpr uint32_t now = 100000;
      seedMood(moods[index], now);
      const int before = timingCalls;
      scheduleNextAutonomousBehavior(now);
#if DESK_BUDDY_DIAGNOSTICS
      assert(!autonomousBehaviorScheduled && nextAutonomousBehaviorAt == 0);
      assert(timingCalls == before);
#else
      assert(timingCalls == before + 1);
      assert(observedMin == minimums[index] && observedExclusiveMax == maximums[index] + 1);
      const uint32_t delay = static_cast<uint32_t>(nextAutonomousBehaviorAt - now);
      assert(delay >= minimums[index] && delay <= maximums[index]);
      if (ticket == TimingTicket::Minimum) assert(delay == minimums[index]);
      if (ticket == TimingTicket::Maximum) assert(delay == maximums[index]);
#endif
    }
  }
}
#if !DESK_BUDDY_DIAGNOSTICS
void expectScheduled(uint32_t now, uint32_t delay) {
  assert(autonomousBehaviorScheduled);
  assert(static_cast<uint32_t>(nextAutonomousBehaviorAt - now) == delay);
}
void testStartupInteractionsAndCompletion() {
  timingTicket = TimingTicket::Minimum;
  beginAt(1000);
  expectScheduled(1000, 28000);
  processBuddyEvent(BuddyEvent::TouchTap, 1001);
  expectScheduled(1001, 28000);
  processBuddyEvent(BuddyEvent::TouchTap, 1002);
  assert(currentMood == BuddyMood::Engaged);
  expectScheduled(1002, 12000);
  for (uint32_t now = 1003; now <= 1005; ++now) processBuddyEvent(BuddyEvent::TouchTap, now);
  assert(currentMood == BuddyMood::Grumpy);
  expectScheduled(1005, 20000);

  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchHold, 100);
  expectScheduled(100, 28000);
  processBuddyEvent(BuddyEvent::TouchHold, 101);
  expectScheduled(101, 12000);
  beginAt(100);
  engagementScore = 35;
  processBuddyEvent(BuddyEvent::SoundDetected, 101);
  assert(currentMood == BuddyMood::Engaged);
  expectScheduled(101, 12000);

  beginAt(100);
  seedMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::IdleTimeout, 100);
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(10100);  // Completion includes one decay tick: 40 -> 35.
  assert(currentMood == BuddyMood::Calm);
  expectScheduled(10100, 28000);

  beginAt(0);
  processBuddyEvent(BuddyEvent::IdleTimeout, 89999);
  faceFinished = true;
  updateBehaviorEngine(90000);  // Completion makes genuine inactivity eligible.
  assert(currentMood == BuddyMood::Sleepy);
  expectScheduled(90000, 45000);
}
void testSleepWakeAndDecayReschedule() {
  timingTicket = TimingTicket::Minimum;
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    beginAt(100000);
    seedMood(mood, 100000);
    processBuddyEvent(BuddyEvent::ButtonPressed, 100001);
    assert(!autonomousBehaviorScheduled);
    updateBehaviorEngine(200000);
    assert(!autonomousBehaviorScheduled && coreState == BuddyCoreState::Sleeping);
    processBuddyEvent(BuddyEvent::ButtonPressed, 200001);
    assert(currentMood == (mood == BuddyMood::Sleepy ? BuddyMood::Calm : mood));
    expectScheduled(200001, mood == BuddyMood::Engaged ? 12000 : mood == BuddyMood::Grumpy ? 20000 : 28000);
  }
  beginAt(0);
  seedMood(BuddyMood::Engaged, 0);
  scheduleNextAutonomousBehavior(0);
  updateBehaviorEngine(10000);
  assert(currentMood == BuddyMood::Calm);
  assert(nextAutonomousBehaviorAt == 38000);  // A crossed mood boundary refreshes timing.
  updateBehaviorEngine(38000);
  assert(activeReaction == BuddyReaction::Generic);
  assert(isAutonomousExpressionForMood(expression, BuddyMood::Calm));
}
void testRolloverDeadlines() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    for (TimingTicket ticket : {TimingTicket::Minimum, TimingTicket::Maximum}) {
      timingTicket = ticket;
      beginAt(anchor);
      seedMood(mood, anchor);
      // Keep the configured mood stable while testing its original deadline.
      if (mood == BuddyMood::Engaged) engagementScore = 100;
      if (mood == BuddyMood::Grumpy) irritationScore = 100;
      scheduleNextAutonomousBehavior(anchor);
      const uint32_t deadline = nextAutonomousBehaviorAt;
      assert(!timeReached(anchor, deadline));
      updateBehaviorEngine(deadline - 1);
      assert(activeReaction == BuddyReaction::Idle);
      assert(nextAutonomousBehaviorAt == deadline);
      updateBehaviorEngine(deadline);
      assert(activeReaction == BuddyReaction::Generic);
      assert(sound == ReactionSound::None);
    }
  }
}

void testMoodTransitionReschedulingAndRandomCalls() {
  timingTicket = TimingTicket::Minimum;
  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  const int beforeInteraction = timingCalls;
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  assert(currentMood == BuddyMood::Engaged);
  expectScheduled(101, 12000);
  assert(autonomousScheduleMood == BuddyMood::Engaged);
  assert(timingCalls == beforeInteraction + 1);
  updateBehaviorEngine(101);
  assert(timingCalls == beforeInteraction + 1);  // No second draw during interaction.

  beginAt(0);
  seedMood(BuddyMood::Engaged, 0);
  engagementScore = 80;
  scheduleNextAutonomousBehavior(0);
  const int beforeSameMood = timingCalls;
  updateBehaviorEngine(10000);
  assert(engagementScore == 75 && currentMood == BuddyMood::Engaged);
  assert(nextAutonomousBehaviorAt == 12000 && timingCalls == beforeSameMood);

  beginAt(0);
  seedMood(BuddyMood::Grumpy, 0);
  engagementScore = 60;
  scheduleNextAutonomousBehavior(0);
  const int beforeRecovery = timingCalls;
  updateBehaviorEngine(10000);
  assert(currentMood == BuddyMood::Grumpy && timingCalls == beforeRecovery);
  updateBehaviorEngine(20000);  // Irritation 55, engagement 50: Engaged.
  assert(currentMood == BuddyMood::Engaged);
  expectScheduled(20000, 12000);
  assert(timingCalls == beforeRecovery + 1);
  assert(activeReaction == BuddyReaction::Idle);  // Discard the due Grumpy deadline.

  beginAt(0);
  const int beforeSleepy = timingCalls;
  updateBehaviorEngine(90000);  // Overdue Calm deadline must not fire.
  assert(currentMood == BuddyMood::Sleepy && activeReaction == BuddyReaction::Idle);
  expectScheduled(90000, 45000);
  assert(timingCalls == beforeSleepy + 1);
  updateBehaviorEngine(90000);
  assert(timingCalls == beforeSleepy + 1);

  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  timingTicket = TimingTicket::Maximum;
  beginAt(anchor);
  seedMood(BuddyMood::Engaged, anchor);
  scheduleNextAutonomousBehavior(anchor);
  const int beforeWrap = timingCalls;
  updateBehaviorEngine(anchor + uint32_t{10000});
  assert(currentMood == BuddyMood::Calm);
  expectScheduled(anchor + uint32_t{10000}, 45000);
  assert(autonomousScheduleMood == BuddyMood::Calm && timingCalls == beforeWrap + 1);
  updateBehaviorEngine(anchor + uint32_t{54999});
  assert(activeReaction == BuddyReaction::Idle);
  updateBehaviorEngine(anchor + uint32_t{55000});
  assert(activeReaction == BuddyReaction::Generic);
}

void testActiveReactionDefersTransitionSchedule() {
  timingTicket = TimingTicket::Minimum;
  beginAt(100);
  seedMood(BuddyMood::Engaged, 100);
  processBuddyEvent(BuddyEvent::IdleTimeout, 100);
  assert(autonomousReactionActive && !autonomousBehaviorScheduled);
  const int beforeAutonomous = timingCalls;
  updateBehaviorEngine(10100);  // Decay to Calm while autonomy is unfinished.
  assert(currentMood == BuddyMood::Calm && autonomousReactionActive);
  assert(!autonomousBehaviorScheduled && timingCalls == beforeAutonomous);
  finishAt(10101);
  expectScheduled(10101, 28000);
  assert(timingCalls == beforeAutonomous + 1);

  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  const uint32_t originalDeadline = nextAutonomousBehaviorAt;
  const int beforeTransient = timingCalls;
  updateBehaviorEngine(20000);  // Interaction still active; do not replace its schedule.
  assert(currentMood == BuddyMood::Calm && activeReaction == BuddyReaction::Generic);
  // Existing busy-deadline handling schedules on a reached deadline. Test transition
  // deferral before that eligibility path by using a maximum Engaged interval below.
  assert(timingCalls == beforeTransient + 1);  // Only the existing busy timeout reset.
  assert(nextAutonomousBehaviorAt != originalDeadline);

  timingTicket = TimingTicket::Maximum;
  beginAt(0);
  seedMood(BuddyMood::Engaged, 0);
  processBuddyEvent(BuddyEvent::SoundDetected, 1);  // 45 points, deadline 24001.
  const int beforeDeferred = timingCalls;
  const uint32_t deferredDeadline = nextAutonomousBehaviorAt;
  updateBehaviorEngine(20000);  // 35 points, Calm; deadline not yet due.
  assert(currentMood == BuddyMood::Calm && activeReaction == BuddyReaction::Generic);
  assert(nextAutonomousBehaviorAt == deferredDeadline && timingCalls == beforeDeferred);
  finishAt(20001);
  expectScheduled(20001, 45000);
  assert(timingCalls == beforeDeferred + 1);
}
#endif
}  // namespace

long random(long) { return 0; }
long random(long minimum, long exclusiveMaximum) {
  assert(minimum < exclusiveMaximum);
  observedMin = minimum;
  observedExclusiveMax = exclusiveMaximum;
  ++timingCalls;
  if (timingTicket == TimingTicket::Maximum) return exclusiveMaximum - 1;
  if (timingTicket == TimingTicket::Middle) return minimum + (exclusiveMaximum - minimum) / 2;
  return minimum;
}
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
#if DESK_BUDDY_DIAGNOSTICS
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requested,
                                  uint8_t, uint8_t &selected) {
  selected = requested == ReactionSound::None ? DIAGNOSTIC_RANDOM_VARIANT : 0;
  startReactionSound(now, requested);
  return true;
}
#endif
void stopReactionSound() {
  sound = ReactionSound::None;
  soundActive = false;
}
bool isSoundEngineActive() { return soundActive; }
void playSleepSound() {}
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}

int main() {
  testRanges();
#if !DESK_BUDDY_DIAGNOSTICS
  testStartupInteractionsAndCompletion();
  testSleepWakeAndDecayReschedule();
  testRolloverDeadlines();
  testMoodTransitionReschedulingAndRandomCalls();
  testActiveReactionDefersTransitionSchedule();
#else
  beginAt(0);
  const int beforeDiagnostic = timingCalls;
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    setDiagnosticMood(mood, 100);
    updateBehaviorEngine(200);
    assert(!autonomousBehaviorScheduled && timingCalls == beforeDiagnostic);
  }
#endif
}

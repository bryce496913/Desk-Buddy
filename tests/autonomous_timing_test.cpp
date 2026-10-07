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
int behaviorCalls = 0;
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
    processBuddyEvent(BuddyEvent::ButtonShortPress, 100001);
    assert(!autonomousBehaviorScheduled);
    updateBehaviorEngine(200000);
    assert(!autonomousBehaviorScheduled && coreState == BuddyCoreState::Sleeping);
    processBuddyEvent(BuddyEvent::ButtonShortPress, 200001);
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

void testLongLifecycleAndRandomIsolation() {
  timingTicket = TimingTicket::Minimum;
  beginAt(0);
  bool sawCalm = false, sawEngaged = false, sawSleepy = false;
  for (uint32_t now = 1000; now <= 300000; now += 1000) {
    if (now == 30000) {
      processBuddyEvent(BuddyEvent::TouchTap, now);
      processBuddyEvent(BuddyEvent::TouchHold, now + 1);
      assert(currentMood == BuddyMood::Engaged);
      expectScheduled(now + 1, 12000);
    }
    faceFinished = activeReaction == BuddyReaction::Generic;
    soundActive = false;
    const int before = timingCalls;
    updateBehaviorEngine(now);
    if (timingCalls != before) {
      assert(timingCalls == before + 1);
      assert(autonomousScheduleMood == currentMood);
      const auto range = autonomousTimingFor(currentMood);
      expectScheduled(now, range.minMs);
      sawCalm |= currentMood == BuddyMood::Calm;
      sawEngaged |= currentMood == BuddyMood::Engaged;
      sawSleepy |= currentMood == BuddyMood::Sleepy;
    }
    const int timingBeforeIdleUpdate = timingCalls;
    const int behaviorBeforeIdleUpdate = behaviorCalls;
    updateBehaviorEngine(now);  // No event, completion, or transition on repeated update.
    assert(timingCalls == timingBeforeIdleUpdate);
    assert(behaviorCalls == behaviorBeforeIdleUpdate);
  }
  assert(sawCalm && sawEngaged && sawSleepy);
  assert(currentMood == BuddyMood::Sleepy);

  beginAt(0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  faceFinished = true;
  soundActive = false;
  updateBehaviorEngine(101);
  engagementScore = 40;
  currentMood = BuddyMood::Engaged;
  scheduleNextAutonomousBehavior(101);
  const auto typeBefore = lastInteractionType;
  const uint32_t atBefore = lastInteractionAt;
  const int drawsBefore = behaviorCalls;
  updateBehaviorEngine(10000);  // Decay crosses Calm, timing draw only.
  assert(currentMood == BuddyMood::Calm);
  assert(lastInteractionType == typeBefore && lastInteractionAt == atBefore && hasLastInteraction);
  assert(behaviorCalls == drawsBefore);
}

void testFrequencyOverlapAndAllInteractionResets() {
  const auto engaged = autonomousTimingFor(BuddyMood::Engaged);
  const auto grumpy = autonomousTimingFor(BuddyMood::Grumpy);
  const auto calm = autonomousTimingFor(BuddyMood::Calm);
  const auto sleepy = autonomousTimingFor(BuddyMood::Sleepy);
  assert(engaged.minMs < grumpy.minMs && grumpy.minMs < calm.minMs && calm.minMs < sleepy.minMs);
  assert(engaged.maxMs < grumpy.maxMs && grumpy.maxMs < calm.maxMs && calm.maxMs < sleepy.maxMs);
  assert(grumpy.minMs <= 30000 && grumpy.maxMs >= 30000);
  assert(calm.minMs <= 30000 && calm.maxMs >= 30000);
  assert(calm.maxMs == sleepy.minMs);  // Intentional shared 45-second boundary.
  timingTicket = TimingTicket::Maximum;
  for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold, BuddyEvent::SoundDetected}) {
    beginAt(0);
    const uint32_t oldDeadline = nextAutonomousBehaviorAt;
    const int before = timingCalls;
    processBuddyEvent(event, 1000);
    assert(nextAutonomousBehaviorAt != oldDeadline);
    assert(autonomousScheduleMood == currentMood && timingCalls == before + 1);
    expectScheduled(1000, autonomousTimingFor(currentMood).maxMs);
    updateBehaviorEngine(1000);
    assert(timingCalls == before + 1);
  }
  beginAt(0);
  processBuddyEvent(BuddyEvent::ButtonShortPress, 1000);
  const int beforeSleep = timingCalls;
  updateBehaviorEngine(300000);
  assert(!autonomousBehaviorScheduled && timingCalls == beforeSleep);
  processBuddyEvent(BuddyEvent::ButtonShortPress, 300001);
  assert(timingCalls == beforeSleep + 1);
  expectScheduled(300001, 45000);
  updateBehaviorEngine(300001);
  assert(timingCalls == beforeSleep + 1 && activeReaction == BuddyReaction::Idle);
}
#endif
}  // namespace

long random(long) { ++behaviorCalls; return 0; }
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
void startReactionSound(uint32_t, ReactionSound requested, BuddyMood) {
  sound = requested;
  soundActive = sound != ReactionSound::None;
  ++soundStarts;
}
#if DESK_BUDDY_DIAGNOSTICS
bool startDiagnosticReactionSound(uint32_t now, ReactionSound requested,
                                  uint8_t, uint8_t &selected, BuddyMood mood) {
  selected = requested == ReactionSound::None ? DIAGNOSTIC_RANDOM_VARIANT : 0;
  startReactionSound(now, requested, mood);
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
  beginAt(100);
  const uint32_t modeDeadline = nextAutonomousBehaviorAt;
  const bool modeScheduled = autonomousBehaviorScheduled;
  const BuddyMood modeScheduleMood = autonomousScheduleMood;
  const int modeTimingCalls = timingCalls;
  const int modeBehaviorCalls = behaviorCalls;
  processBuddyEvent(BuddyEvent::ButtonLongPress, 101);
  assert(getBuddySoundMode() == BuddySoundMode::Quiet);
  assert(nextAutonomousBehaviorAt == modeDeadline && autonomousBehaviorScheduled == modeScheduled &&
         autonomousScheduleMood == modeScheduleMood && timingCalls == modeTimingCalls && behaviorCalls == modeBehaviorCalls);
  processBuddyEvent(BuddyEvent::ButtonLongPress, 102);
  assert(getBuddySoundMode() == BuddySoundMode::Normal && nextAutonomousBehaviorAt == modeDeadline);

  testRanges();
#if !DESK_BUDDY_DIAGNOSTICS
  testStartupInteractionsAndCompletion();
  testSleepWakeAndDecayReschedule();
  testRolloverDeadlines();
  testMoodTransitionReschedulingAndRandomCalls();
  testActiveReactionDefersTransitionSchedule();
  testLongLifecycleAndRandomIsolation();
  testFrequencyOverlapAndAllInteractionResets();
#else
  beginAt(0);
  const int beforeDiagnostic = timingCalls;
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    setDiagnosticMood(mood, 100);
    updateBehaviorEngine(200);
    assert(!autonomousBehaviorScheduled && timingCalls == beforeDiagnostic);
    const auto state = getDiagnosticAutonomousTimingState(200);
    assert(!state.scheduled && state.remainingMs == 0);
  }
  // Inspect a synthetic pending schedule only through the real read-only getter.
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 500;
  autonomousBehaviorScheduled = true;
  autonomousScheduleMood = BuddyMood::Engaged;
  nextAutonomousBehaviorAt = anchor + uint32_t{12000};
  const auto state = getDiagnosticAutonomousTimingState(anchor + uint32_t{1000});
  assert(state.scheduled && state.scheduleMood == BuddyMood::Engaged && state.remainingMs == 11000);
  assert(state.minMs == 12000 && state.maxMs == 24000);
  assert(getDiagnosticAutonomousTimingState(nextAutonomousBehaviorAt).remainingMs == 0);
  assert(getDiagnosticAutonomousTimingState(nextAutonomousBehaviorAt + 1).remainingMs == 0);
  assert(nextAutonomousBehaviorAt == anchor + uint32_t{12000} && timingCalls == beforeDiagnostic);
#endif
}

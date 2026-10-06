#include "autonomous_test_helpers.h"
#include <cassert>
#include <cstdint>
#include <limits>

#include "BehaviorEngine.h"
#include "FaceRenderer.h"
#include "SoundEngine.h"

static_assert(DESK_BUDDY_DIAGNOSTICS == 1,
              "lifecycle telemetry requires diagnostics mode");

BuddyMood getTestAutonomousSelectionMood();

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

void buildGrumpy() {
  for (uint32_t now = 100; now <= 104; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  expectMood(104, BuddyMood::Grumpy, 60, 75, 0);
}

void testFriendlyEngagementThenCalm() {
  beginAt();
  expectMood(0, BuddyMood::Calm, 0, 0, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  expectMood(100, BuddyMood::Calm, 20, 0, 0);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  processBuddyEvent(BuddyEvent::TouchTap, 200);
  expectMood(200, BuddyMood::Engaged, 45, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  finishAt(201);
  updateBehaviorEngine(10000);
  expectMood(10000, BuddyMood::Engaged, 40, 0, 9800);
  updateBehaviorEngine(20000);
  expectMood(20000, BuddyMood::Calm, 35, 0, 19800);
}

void testHoldMoodContexts() {
  const BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged,
                            BuddyMood::Grumpy, BuddyMood::Sleepy};
  const uint8_t engagement[] = {30, 70, 30, 30};
  const uint8_t irritation[] = {0, 0, 45, 0};
  for (uint8_t index = 0; index < 4; ++index) {
    beginAt();
    setDiagnosticMood(moods[index], 100);
    processBuddyEvent(BuddyEvent::TouchHold, 200);
    const bool curious = moods[index] == BuddyMood::Grumpy ||
                         moods[index] == BuddyMood::Sleepy;
    expectReaction(curious ? FaceExpression::Curious : FaceExpression::Happy,
                   curious ? ReactionSound::Curious : ReactionSound::Happy);
    expectMood(200, index == 1 ? BuddyMood::Engaged : BuddyMood::Calm,
               engagement[index], irritation[index], 0);
    // Grumpy and Sleepy become Calm, but their arrival mood selects Curious.
  }

  beginAt();
  updateBehaviorEngine(90000);  // Naturally Sleepy, with real inactivity.
  expectMood(90000, BuddyMood::Sleepy, 0, 0, 90000);
  processBuddyEvent(BuddyEvent::TouchHold, 90001);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(90001, BuddyMood::Calm, 30, 0, 0);
  finishAt(90002);
  updateBehaviorEngine(180000);
  expectMood(180000, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(180001);
  expectMood(180001, BuddyMood::Sleepy, 0, 0, 90000);
}

void testSeparateHoldsAndGrumpyRecovery() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchHold, 100);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(100, BuddyMood::Calm, 30, 0, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 200);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(200, BuddyMood::Engaged, 60, 0, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 300);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(300, BuddyMood::Engaged, 90, 0, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 400);
  expectMood(400, BuddyMood::Engaged, 100, 0, 0);
  assert(faceStarts == 4 && soundStarts == 4);

  beginAt();
  buildGrumpy();  // Natural scores: engagement 60, irritation 75.
  processBuddyEvent(BuddyEvent::TouchHold, 200);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(200, BuddyMood::Grumpy, 90, 60, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 300);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(300, BuddyMood::Engaged, 100, 45, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 400);
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(400, BuddyMood::Engaged, 100, 30, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 500);
  expectMood(500, BuddyMood::Engaged, 100, 15, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 600);
  expectMood(600, BuddyMood::Engaged, 100, 0, 0);
  processBuddyEvent(BuddyEvent::TouchHold, 700);
  expectMood(700, BuddyMood::Engaged, 100, 0, 0);
}

void testHoldLeavesTapHistoryAlone() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchHold, 200);
  expectMood(200, BuddyMood::Engaged, 50, 0, 0);
  setDiagnosticMood(BuddyMood::Calm, 201);  // Remove mood ambiguity.
  processBuddyEvent(BuddyEvent::TouchTap, 300);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);  // Tap #2.
  expectMood(300, BuddyMood::Calm, 25, 0, 0);

  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchHold, 3000);
  setDiagnosticMood(BuddyMood::Calm, 3001);
  processBuddyEvent(BuddyEvent::TouchTap, 6201);  // >6000 since Tap, <6000 since Hold.
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(6201, BuddyMood::Calm, 20, 0, 0);
}

void testHoldLeavesSoundHistoryAndDecayAlone() {
  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 100);
  processBuddyEvent(BuddyEvent::TouchHold, 200);
  processBuddyEvent(BuddyEvent::SoundDetected, 300);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  expectMood(300, BuddyMood::Engaged, 40, 0, 0);

  beginAt();
  processBuddyEvent(BuddyEvent::SoundDetected, 100);
  processBuddyEvent(BuddyEvent::TouchHold, 5000);
  // No update yet, so no decay; scores remain below Engaged at arrival.
  processBuddyEvent(BuddyEvent::SoundDetected, 10101);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
  expectMood(10101, BuddyMood::Engaged, 40, 0, 0);

  beginAt();
  buildGrumpy();
  processBuddyEvent(BuddyEvent::TouchHold, 9999);
  expectMood(9999, BuddyMood::Grumpy, 90, 60, 0);
  finishAt(10000);  // Hold did not restart the original decay clock.
  expectMood(10000, BuddyMood::Engaged, 85, 50, 1);
  updateBehaviorEngine(20000);
  expectMood(20000, BuddyMood::Engaged, 80, 40, 10001);
}

void testSleepingIgnoresHold() {
  beginAt();
  buildGrumpy();
  processBuddyEvent(BuddyEvent::ButtonPressed, 200);
  const int facesBefore = faceStarts;
  const int soundsBefore = soundStarts;
  processBuddyEvent(BuddyEvent::TouchHold, 300);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  assert(getBuddyReaction() == BuddyReaction::Idle);
  expectMood(300, BuddyMood::Grumpy, 60, 75, 0);
  assert(faceStarts == facesBefore && soundStarts == soundsBefore);
  processBuddyEvent(BuddyEvent::ButtonPressed, 400);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  expectMood(400, BuddyMood::Grumpy, 60, 75, 0);
}

void testHoldAcrossRollover() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  beginAt(anchor);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + 100);
  processBuddyEvent(BuddyEvent::TouchHold, anchor + uint32_t{5500});
  expectMood(anchor + uint32_t{5500}, BuddyMood::Engaged, 50, 0, 0);
  setDiagnosticMood(BuddyMood::Calm, anchor + uint32_t{5501});
  processBuddyEvent(BuddyEvent::TouchTap, anchor + uint32_t{6201});
  expectReaction(FaceExpression::Happy, ReactionSound::Happy);
  expectMood(anchor + uint32_t{6201}, BuddyMood::Calm, 20, 0, 0);
}

void testExcessiveAttentionAndRecovery() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  expectMood(101, BuddyMood::Engaged, 45, 0, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 102);
  expectMood(102, BuddyMood::Engaged, 50, 25, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 103);
  expectMood(103, BuddyMood::Engaged, 55, 50, 0);
  processBuddyEvent(BuddyEvent::TouchTap, 104);
  expectMood(104, BuddyMood::Grumpy, 60, 75, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  finishAt(105);
  updateBehaviorEngine(10000);
  expectMood(10000, BuddyMood::Grumpy, 55, 65, 9896);
  updateBehaviorEngine(20000);
  expectMood(20000, BuddyMood::Engaged, 50, 55, 19896);
  updateBehaviorEngine(50000);
  expectMood(50000, BuddyMood::Calm, 35, 25, 49896);
}

void testInactivityThroughAutonomousReactions() {
  beginAt();
  // Diagnostics suppress scheduling; explicit IdleTimeout uses the same real
  // autonomous path. The production suite also covers scheduled autonomy.
  const uint32_t deadlines[] = {20000, 40000, 60000, 80000};
  for (uint32_t now : deadlines) {
    updateBehaviorEngine(now);
    processBuddyEvent(BuddyEvent::IdleTimeout, now);
    assert(isAutonomousExpressionForMood(expression, getBuddyMood()));
    expectMood(now, BuddyMood::Calm, 0, 0, now);
    finishAt(now + 1);
    expectMood(now + 1, BuddyMood::Calm, 0, 0, now + 1);
  }
  updateBehaviorEngine(89999);
  expectMood(89999, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(90000);
  expectMood(90000, BuddyMood::Sleepy, 0, 0, 90000);
}

void testSleepyTouchAndSoundAlertness() {
  beginAt();
  updateBehaviorEngine(90000);
  processBuddyEvent(BuddyEvent::TouchTap, 90001);
  expectMood(90001, BuddyMood::Calm, 20, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  processBuddyEvent(BuddyEvent::TouchTap, 90002);
  expectMood(90002, BuddyMood::Engaged, 45, 0, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);

  beginAt();
  updateBehaviorEngine(90000);
  expectMood(90000, BuddyMood::Sleepy, 0, 0, 90000);
  processBuddyEvent(BuddyEvent::SoundDetected, 90001);
  expectMood(90001, BuddyMood::Calm, 5, 0, 0);
  expectReaction(FaceExpression::Startled, ReactionSound::Startled);
}

void testSoundRestartsInactivity() {
  beginAt();
  updateBehaviorEngine(89998);
  processBuddyEvent(BuddyEvent::SoundDetected, 89999);
  expectMood(89999, BuddyMood::Calm, 5, 0, 0);
  finishAt(90000);  // One decay tick removes the five sound points.
  expectMood(90000, BuddyMood::Calm, 0, 0, 1);
  updateBehaviorEngine(179998);
  expectMood(179998, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(179999);
  expectMood(179999, BuddyMood::Sleepy, 0, 0, 90000);
}

void testPhysicalSleepPausesScoresAndInactivity() {
  beginAt();
  buildGrumpy();
  processBuddyEvent(BuddyEvent::ButtonPressed, 200);
  assert(getBuddyCoreState() == BuddyCoreState::Sleeping);
  constexpr uint32_t wakeAt = 8 * 60 * 60 * 1000;
  updateBehaviorEngine(wakeAt - 1);
  expectMood(wakeAt - 1, BuddyMood::Grumpy, 60, 75, 0);
  processBuddyEvent(BuddyEvent::ButtonPressed, wakeAt);
  updateBehaviorEngine(wakeAt);
  assert(getBuddyCoreState() == BuddyCoreState::Awake);
  expectMood(wakeAt, BuddyMood::Grumpy, 60, 75, 0);
  updateBehaviorEngine(wakeAt + 10000);
  expectMood(wakeAt + 10000, BuddyMood::Grumpy, 55, 65, 10000);
  updateBehaviorEngine(wakeAt + 20000);
  expectMood(wakeAt + 20000, BuddyMood::Engaged, 50, 55, 20000);
}

void testRolloverForDecayAndInactivity() {
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 5000;
  beginAt(anchor);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + 1);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + 2);
  finishAt(anchor + 3);
  updateBehaviorEngine(anchor + uint32_t{10000});
  expectMood(anchor + uint32_t{10000}, BuddyMood::Engaged, 40, 0, 9998);
  updateBehaviorEngine(anchor + uint32_t{20000});
  expectMood(anchor + uint32_t{20000}, BuddyMood::Calm, 35, 0, 19998);

  beginAt(anchor);
  updateBehaviorEngine(anchor + uint32_t{89999});
  expectMood(anchor + uint32_t{89999}, BuddyMood::Calm, 0, 0, 89999);
  updateBehaviorEngine(anchor + uint32_t{90000});
  expectMood(anchor + uint32_t{90000}, BuddyMood::Sleepy, 0, 0, 90000);
}

void testScoreBoundsAndReadOnlyTelemetry() {
  beginAt();
  for (uint32_t now = 1; now <= 1000; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
    const DiagnosticMoodState state = getDiagnosticMoodState(now);
    assert(state.engagementScore <= 100 && state.irritationScore <= 100);
  }
  expectMood(1000, BuddyMood::Grumpy, 100, 100, 0);
  const int facesBeforeRead = faceStarts;
  const int soundsBeforeRead = soundStarts;
  // Reading at a much later timestamp must not apply decay or mutate anchors.
  expectMood(1000000, BuddyMood::Grumpy, 100, 100, 999000);
  expectMood(1000, BuddyMood::Grumpy, 100, 100, 0);
  assert(faceStarts == facesBeforeRead && soundStarts == soundsBeforeRead);
  assert(getBuddyReaction() == BuddyReaction::Generic);

  beginAt();
  for (uint32_t now = 1; now <= 1000; ++now) {
    processBuddyEvent(BuddyEvent::SoundDetected, now);
    const DiagnosticMoodState state = getDiagnosticMoodState(now);
    assert(state.engagementScore <= 100 && state.irritationScore == 0);
  }
  expectMood(1000, BuddyMood::Engaged, 100, 0, 0);
}

void testGrumpyReactionContext() {
  beginAt();
  buildGrumpy();
  finishAt(105);
  processBuddyEvent(BuddyEvent::TouchTap, 6105);  // Streak expired, mood retained.
  expectMood(6105, BuddyMood::Grumpy, 80, 75, 0);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
}

void testAutomaticMoodContextForIdenticalFirstSounds() {
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  finishAt(102);
  updateBehaviorEngine(6102);  // Touch streak expired, engagement retained.
  expectMood(6102, BuddyMood::Engaged, 45, 0, 6001);
  processBuddyEvent(BuddyEvent::SoundDetected, 6103);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(6103, BuddyMood::Engaged, 50, 0, 0);

  for (uint32_t now = 6104; now <= 6108; ++now) {
    processBuddyEvent(BuddyEvent::TouchTap, now);
  }
  expectMood(6108, BuddyMood::Grumpy, 100, 75, 0);
  finishAt(6109);
  updateBehaviorEngine(16104);  // One decay tick; sound window expired.
  expectMood(16104, BuddyMood::Grumpy, 95, 65, 9996);
  processBuddyEvent(BuddyEvent::SoundDetected, 16104);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);
  expectMood(16104, BuddyMood::Grumpy, 100, 65, 0);
}

void expectRecent(uint32_t now, DiagnosticRecentInteractionType type,
                  uint32_t age, bool recent) {
  const DiagnosticRecentInteractionContext context =
      getDiagnosticRecentInteractionContext(now);
  assert(context.type == type);
  assert(context.ageMs == age);
  assert(context.recent == recent);
}

void testRecentInteractionRecordingAndWindow() {
  using Type = DiagnosticRecentInteractionType;
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 0);  // Timestamp zero is valid history.
  expectRecent(0, Type::TouchTap, 0, true);
  beginAt(100);
  expectRecent(100, Type::None, 0, false);
  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  expectRecent(1000, Type::TouchTap, 0, true);
  expectRecent(6000, Type::TouchTap, 5000, true);
  expectRecent(6001, Type::TouchTap, 5001, false);
  // Snapshot queries neither erase expired state nor mutate its timestamp.
  expectRecent(1000, Type::TouchTap, 0, true);
  processBuddyEvent(BuddyEvent::TouchTap, 1100);
  expectRecent(1100, Type::TouchTap, 0, true);
  expectRecent(1200, Type::TouchTap, 100, true);
  processBuddyEvent(BuddyEvent::TouchHold, 1200);
  expectRecent(1200, Type::TouchHold, 0, true);
  processBuddyEvent(BuddyEvent::SoundDetected, 1300);
  expectRecent(1300, Type::Sound, 0, true);
  processBuddyEvent(BuddyEvent::SoundDetected, 1400);
  expectRecent(1500, Type::Sound, 100, true);
  // All events above replace an unfinished transient reaction and still record.
  assert(faceStarts == 5 && soundStarts == 5);
  finishAt(1500);
  expectRecent(1500, Type::Sound, 100, true);
  updateBehaviorEngine(10000);  // Decay and expiry do not erase the record.
  expectRecent(10000, Type::Sound, 8600, false);
  beginAt(10001);
  expectRecent(10001, Type::None, 0, false);
}

void testAutonomousShowcaseIsolation() {
  const DiagnosticAutonomousBehavior behaviors[] = {
      DiagnosticAutonomousBehavior::Curious, DiagnosticAutonomousBehavior::Daydreaming,
      DiagnosticAutonomousBehavior::SideGlance, DiagnosticAutonomousBehavior::Bored,
      DiagnosticAutonomousBehavior::SuspiciousGlance, DiagnosticAutonomousBehavior::AnnoyedSquint,
      DiagnosticAutonomousBehavior::SleepyDrift, DiagnosticAutonomousBehavior::ExcitedScanning};
  const FaceExpression expressions[] = {FaceExpression::Curious, FaceExpression::Daydreaming,
      FaceExpression::SideGlance, FaceExpression::Bored, FaceExpression::SuspiciousGlance,
      FaceExpression::AnnoyedSquint, FaceExpression::SleepyDrift, FaceExpression::ExcitedScanning};
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::SoundDetected, 101);
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    setDiagnosticMood(mood, 102);
    for (uint8_t index = 0; index < 8; ++index) {
      const auto before = getDiagnosticMoodState(200);
      const auto previous = getDiagnosticRecentInteractionContext(200);
      assert(triggerDiagnosticAutonomousBehavior(behaviors[index], 200));
      expectReaction(expressions[index], ReactionSound::None);
      assert(!isSoundEngineActive());
      const auto after = getDiagnosticMoodState(200);
      const auto context = getDiagnosticRecentInteractionContext(200);
      assert(before.mood == after.mood && before.engagementScore == after.engagementScore);
      assert(before.irritationScore == after.irritationScore && before.inactivityMs == after.inactivityMs);
      assert(previous.type == context.type && previous.ageMs == context.ageMs && previous.recent == context.recent);
    }
  }
  setDiagnosticMood(BuddyMood::Calm, 201);
  processBuddyEvent(BuddyEvent::TouchTap, 202);
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);
  expectMood(202, BuddyMood::Calm, 25, 0, 0);  // Tap #2.
  processBuddyEvent(BuddyEvent::SoundDetected, 203);
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.
  const int facesBefore = faceStarts;
  assert(!triggerDiagnosticAutonomousBehavior(static_cast<DiagnosticAutonomousBehavior>(255), 204));
  assert(faceStarts == facesBefore);
  processBuddyEvent(BuddyEvent::ButtonPressed, 205);
  for (auto behavior : behaviors) assert(!triggerDiagnosticAutonomousBehavior(behavior, 206));
  assert(faceStarts == facesBefore);

  beginAt();
  processBuddyEvent(BuddyEvent::IdleTimeout, 100);
  const FaceExpression first = expression;
  assert(triggerDiagnosticAutonomousBehavior(DiagnosticAutonomousBehavior::Bored, 101));
  finishAt(102);
  processBuddyEvent(BuddyEvent::IdleTimeout, 103);
  assert(expression != first);  // Showcase never changes selection history.
  assert(isAutonomousExpressionForMood(expression, getBuddyMood()));
}

void testAutonomousSelectionIsolation() {
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy}) {
    beginAt();
    processBuddyEvent(BuddyEvent::TouchTap, 100);
    processBuddyEvent(BuddyEvent::SoundDetected, 101);
    finishAt(102);
    setDiagnosticMood(mood, 103);
    const auto before = getDiagnosticMoodState(104);
    const auto previous = getDiagnosticRecentInteractionContext(104);
    processBuddyEvent(BuddyEvent::IdleTimeout, 104);
    assert(getTestAutonomousSelectionMood() == mood);
    assert(isAutonomousExpressionForMood(expression, getBuddyMood()));
    assert(sound == ReactionSound::None);
    const auto after = getDiagnosticMoodState(104);
    const auto context = getDiagnosticRecentInteractionContext(104);
    assert(before.mood == after.mood && before.engagementScore == after.engagementScore);
    assert(before.irritationScore == after.irritationScore && before.inactivityMs == after.inactivityMs);
    assert(previous.type == context.type && previous.ageMs == context.ageMs && previous.recent == context.recent);
    const FaceExpression first = expression;
    finishAt(105);
    processBuddyEvent(BuddyEvent::IdleTimeout, 106);
    assert(expression != first);  // Exclusion preserves immediate-repeat avoidance.
    finishAt(107);
    setDiagnosticMood(BuddyMood::Calm, 108);
    processBuddyEvent(BuddyEvent::TouchTap, 109);
    expectReaction(FaceExpression::Curious, ReactionSound::Curious);  // Tap #2.
    expectMood(109, BuddyMood::Calm, 25, 0, 0);
    processBuddyEvent(BuddyEvent::SoundDetected, 110);
    expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.
  }

  // Direct timeouts apply pending decay before selecting, even without update().
  beginAt();
  processBuddyEvent(BuddyEvent::TouchTap, 100);
  processBuddyEvent(BuddyEvent::TouchTap, 101);
  finishAt(102);
  processBuddyEvent(BuddyEvent::IdleTimeout, 20000);
  assert(getTestAutonomousSelectionMood() == BuddyMood::Calm);
  expectMood(20000, BuddyMood::Calm, 35, 0, 19899);
}

void testRecentInteractionExclusionsAndSleep() {
  using Type = DiagnosticRecentInteractionType;
  beginAt();
  setDiagnosticMood(BuddyMood::Engaged, 100);
  uint8_t selected = DIAGNOSTIC_RANDOM_VARIANT;
  assert(triggerDiagnosticReaction(DiagnosticReaction::Happy,
      DiagnosticSoundVariant::Random, 200, selected));
  expectRecent(200, Type::None, 0, false);
  finishAt(201);
  processBuddyEvent(BuddyEvent::IdleTimeout, 202);
  expectRecent(202, Type::None, 0, false);

  processBuddyEvent(BuddyEvent::TouchTap, 1000);
  finishAt(1001);
  processBuddyEvent(BuddyEvent::IdleTimeout, 1100);
  expectRecent(1100, Type::TouchTap, 100, true);
  assert(isAutonomousExpressionForMood(expression, getBuddyMood()));
  finishAt(1101);
  processBuddyEvent(BuddyEvent::IdleTimeout, 1200);
  expectRecent(1200, Type::TouchTap, 200, true);
  assert(isAutonomousExpressionForMood(expression, getBuddyMood()));
  // Consecutive autonomous reactions exercise both existing personalities.
  finishAt(1201);
  setDiagnosticMood(BuddyMood::Grumpy, 1300);
  expectRecent(1300, Type::TouchTap, 300, true);
  assert(triggerDiagnosticReaction(DiagnosticReaction::Startled,
      DiagnosticSoundVariant::Variant1, 1400, selected));
  expectRecent(1400, Type::TouchTap, 400, true);
  assert(triggerDiagnosticReaction(DiagnosticReaction::Normal,
      DiagnosticSoundVariant::Random, 1500, selected));
  expectRecent(1500, Type::TouchTap, 500, true);

  processBuddyEvent(BuddyEvent::ButtonPressed, 1600);
  expectRecent(1600, Type::None, 0, false);
  for (BuddyEvent event : {BuddyEvent::TouchTap, BuddyEvent::TouchHold,
                          BuddyEvent::SoundDetected, BuddyEvent::IdleTimeout}) {
    processBuddyEvent(event, 1601);
    expectRecent(1601, Type::None, 0, false);
  }
  processBuddyEvent(BuddyEvent::ButtonPressed, 1602);  // Even very short sleep clears.
  expectRecent(1602, Type::None, 0, false);
  processBuddyEvent(BuddyEvent::SoundDetected, 1603);
  expectRecent(1603, Type::Sound, 0, true);
}

void testRecentInteractionRolloverAndHistoryIndependence() {
  using Type = DiagnosticRecentInteractionType;
  constexpr uint32_t anchor = std::numeric_limits<uint32_t>::max() - 1000;
  beginAt(anchor);
  processBuddyEvent(BuddyEvent::TouchTap, anchor);
  expectRecent(anchor + uint32_t{5000}, Type::TouchTap, 5000, true);
  expectRecent(anchor + uint32_t{5001}, Type::TouchTap, 5001, false);
  processBuddyEvent(BuddyEvent::TouchHold, anchor + uint32_t{5100});
  expectRecent(anchor + uint32_t{5100}, Type::TouchHold, 0, true);
  setDiagnosticMood(BuddyMood::Calm, anchor + uint32_t{5101});
  processBuddyEvent(BuddyEvent::TouchTap, anchor + uint32_t{5200});
  expectReaction(FaceExpression::Curious, ReactionSound::Curious);  // Tap #2.
  expectMood(anchor + uint32_t{5200}, BuddyMood::Calm, 25, 0, 0);
  expectRecent(anchor + uint32_t{5200}, Type::TouchTap, 0, true);
  processBuddyEvent(BuddyEvent::SoundDetected, anchor + uint32_t{5300});
  expectReaction(FaceExpression::Confused, ReactionSound::Confused);
  expectRecent(anchor + uint32_t{5300}, Type::Sound, 0, true);
  processBuddyEvent(BuddyEvent::TouchTap, anchor + uint32_t{5400});
  expectMood(anchor + uint32_t{5400}, BuddyMood::Calm, 35, 25, 0);  // Tap #3.
  processBuddyEvent(BuddyEvent::SoundDetected, anchor + uint32_t{5500});
  expectReaction(FaceExpression::Suspicious, ReactionSound::Suspicious);  // Sound #2.
  expectRecent(anchor + uint32_t{5500}, Type::Sound, 0, true);
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
  testFriendlyEngagementThenCalm();
  testHoldMoodContexts();
  testSeparateHoldsAndGrumpyRecovery();
  testHoldLeavesTapHistoryAlone();
  testHoldLeavesSoundHistoryAndDecayAlone();
  testSleepingIgnoresHold();
  testHoldAcrossRollover();
  testExcessiveAttentionAndRecovery();
  testInactivityThroughAutonomousReactions();
  testSleepyTouchAndSoundAlertness();
  testSoundRestartsInactivity();
  testPhysicalSleepPausesScoresAndInactivity();
  testRolloverForDecayAndInactivity();
  testScoreBoundsAndReadOnlyTelemetry();
  testGrumpyReactionContext();
  testAutomaticMoodContextForIdenticalFirstSounds();
  testRecentInteractionRecordingAndWindow();
  testAutonomousShowcaseIsolation();
  testAutonomousSelectionIsolation();
  testRecentInteractionExclusionsAndSleep();
  testRecentInteractionRolloverAndHistoryIndependence();
  return 0;
}

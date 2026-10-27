#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
using std::max;
template <typename T> T constrain(T value, T low, T high) {
  return std::clamp(value, low, high);
}
void analogWrite(uint8_t, int) {}
void analogWriteFreq(int) {}
void analogWriteRange(int) {}
void pinMode(uint8_t, uint8_t) {}
long random(long low, long) { return low; }
long random(long) { return 0; }
// Include the actual renderer so private procedural geometry can be inspected.
#include "../FaceRenderer.cpp"

void equalParams(const EyeExpressionParams& actual, const EyeExpressionParams& expected) {
  assert(actual.topLid == expected.topLid && actual.bottomLid == expected.bottomLid);
  assert(actual.irisRadius == expected.irisRadius && actual.pupilRadius == expected.pupilRadius);
  assert(actual.pupilBiasX == expected.pupilBiasX && actual.pupilBiasY == expected.pupilBiasY);
  assert(actual.reactiveIris == expected.reactiveIris && actual.showSpark == expected.showSpark);
}
void near(float actual, float expected) {
  assert(std::fabs(actual - expected) < 0.00001f);
}
EyeExpressionParams restParams(BuddyMood mood) {
  const MoodRestPose pose = moodRestPose(mood);
  return {pose.topLid, pose.bottomLid, 18 + int(lroundf(pose.irisDelta)),
          8 + int(lroundf(pose.pupilDelta)), int(lroundf(pose.biasX)),
          int(lroundf(pose.biasY)), false, false};
}
void reset(uint32_t now, BuddyMood mood) {
  reactionExitTransitionActive = reactionEntryTransitionActive = false;
  moodMomentActive = snapMoodOnNextUpdate = false;
  wakeStretchActive = fallingAsleep = false;
  hasRenderedEyes = blinkActive = false;
  lidAmount = lidEased = 0;
  pupilX = pupilY = pupilTargetX = pupilTargetY = 0;
  faceMood = shownMood = momentMood = mood;
  restPose = moodRestPose(mood);
  idleSettledAt = now - 10000;
  nextBlinkAt = nextDrowsyAt = nextLookAt = now + 60000;
  drowsyUntil = now;
  lastFrameAt = now;
}
void idle(uint32_t now) {
  updateFaceRenderer(now, BuddyCoreState::Awake, BuddyReaction::Idle);
}
void checkBounds(const EyeExpressionParams& p) {
  assert(p.topLid >= 0 && p.topLid <= 1 && p.bottomLid >= 0 && p.bottomLid <= 1);
  assert(p.topLid + p.bottomLid < 0.93f * 2);  // Moments never read as closed eyes.
  // Pupil travel plus iris radius stays inside the drawn eye.
  assert(p.irisRadius >= 15 && p.irisRadius <= 21 && p.pupilRadius >= 6 && p.pupilRadius < p.irisRadius);
  assert(17 + p.irisRadius <= eyeW / 2 && 24 + p.irisRadius <= eyeH / 2);
}

int main() {
  static_assert(MOOD_MOMENT_DELAY_MS == 400 && MOOD_MOMENT_MS == 900 && SLEEPY_MOOD_MOMENT_MS == 1300);
  // Calm preserves the V2 neutral idle geometry exactly, including blinks.
  reset(1000, BuddyMood::Calm);
  for (float lid : {0.0f, 0.37f, 1.0f}) {
    lidAmount = lid;
    equalParams(expressionParams(FaceExpression::Normal, true, 1000),
                EyeExpressionParams{lid, lid, 18, 8, 0, 0, false, false});
  }
  // Each mood rest pose is visible, bounded, and still closes fully on a blink.
  for (BuddyMood mood : {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    reset(1000, mood);
    equalParams(expressionParams(FaceExpression::Normal, true, 1000), restParams(mood));
    lidAmount = 1;
    const auto closed = expressionParams(FaceExpression::Normal, false, 1000);
    near(closed.topLid, 1);
    near(closed.bottomLid, 1);
  }
  assert(restParams(BuddyMood::Engaged).pupilBiasY < 0 && restParams(BuddyMood::Engaged).irisRadius > 18);
  assert(restParams(BuddyMood::Grumpy).topLid > 0.1f && restParams(BuddyMood::Sleepy).topLid >
         restParams(BuddyMood::Grumpy).topLid);

  // Every from -> to mood moment glides from the old rest to the new one,
  // peaks with its gesture mid-way, and stays drawable throughout.
  const BuddyMood moods[] = {BuddyMood::Calm, BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy};
  for (BuddyMood from : moods) {
    for (BuddyMood to : moods) {
      if (from == to) continue;
      reset(5000, from);
      nextBlinkAt = nextDrowsyAt = nextLookAt = 5000;  // Expired, but suppressed.
      setFaceMood(to);
      idle(5000);
      assert(moodMomentActive && momentMood == to && shownMood == from);
      equalParams(expressionParams(FaceExpression::Normal, true, 5000), restParams(from));
      const uint32_t duration = moodMomentDuration(to);
      for (uint32_t t = 0; t <= duration; t += 50) {
        checkBounds(expressionParams(FaceExpression::Normal, true, 5000 + t));
        checkBounds(expressionParams(FaceExpression::Normal, false, 5000 + t));
      }
      const auto peak = moodMomentAccent(5000 + duration / 2, true);
      if (to == BuddyMood::Engaged) assert(peak.biasY < -3.9f && peak.irisDelta > 1.9f);
      else assert(peak.topLid > 0.17f);
      lastFrameAt = 5000;
      idle(5000 + duration / 2);
      assert(!blinkActive && pupilTargetX == 0 && pupilTargetY == 0);
      idle(5000 + duration - 1);
      assert(moodMomentActive);
      idle(5000 + duration);
      assert(!moodMomentActive && shownMood == to);
      equalParams(expressionParams(FaceExpression::Normal, true, 5000 + duration), restParams(to));
      assert(nextBlinkAt > 5000 + duration && nextLookAt > 5000 + duration);
    }
  }

  // A mood change during a reaction waits for exit plus a short settle beat.
  reset(10000, BuddyMood::Calm);
  renderFrame(10000, BuddyCoreState::Awake, BuddyReaction::Idle);
  startFaceReaction(10000, FaceExpression::Happy);
  setFaceMood(BuddyMood::Engaged);
  updateFaceRenderer(10500, BuddyCoreState::Awake, BuddyReaction::Generic);
  assert(!moodMomentActive);
  finishFaceReaction(11800);
  equalParams(exitToLeft, restParams(BuddyMood::Calm));
  idle(11900);
  assert(reactionExitTransitionActive && !moodMomentActive);
  idle(12020);
  assert(!reactionExitTransitionActive && !moodMomentActive && idleSettledAt == 12020);
  equalParams(visualExpressionParams(12020, BuddyReaction::Idle, true), exitToLeft);
  idle(12419);
  assert(!moodMomentActive);
  idle(12420);
  assert(moodMomentActive && momentMood == BuddyMood::Engaged);

  // A reaction interrupting a moment freezes the glide, then the moment replays.
  lastFrameAt = 12420;
  idle(12870);
  const MoodRestPose midway = currentRestPose(12870);
  assert(midway.biasY < 0 && midway.biasY > -3);
  startFaceReaction(12870, FaceExpression::Curious);
  assert(!moodMomentActive && shownMood == BuddyMood::Calm);
  equalParams(entryFromLeft, renderedLeft);
  near(restPose.biasY, midway.biasY);
  finishFaceReaction(14670);
  idle(14890);
  assert(!reactionExitTransitionActive);
  idle(15290);
  assert(moodMomentActive);
  near(momentFrom.biasY, midway.biasY);
  idle(15290 + MOOD_MOMENT_MS);
  assert(shownMood == BuddyMood::Engaged);

  // Diagnostic Normal finishes from idle mid-moment without a jump.
  reset(20000, BuddyMood::Calm);
  setFaceMood(BuddyMood::Grumpy);
  idle(20000);
  renderFrame(20300, BuddyCoreState::Awake, BuddyReaction::Idle);
  const auto before = renderedLeft;
  finishFaceReaction(20300);
  assert(!moodMomentActive);
  equalParams(exitFromLeft, before);

  // Sleep snaps to the mood silently, and wake opens straight into it.
  reset(30000, BuddyMood::Engaged);
  enterSleepFace(30000);
  setFaceMood(BuddyMood::Sleepy);
  updateFaceRenderer(30100, BuddyCoreState::Sleeping, BuddyReaction::Idle);
  assert(!moodMomentActive && shownMood == BuddyMood::Sleepy);
  wakeFace(31000);
  setFaceMood(BuddyMood::Calm);
  idle(31000);
  assert(!moodMomentActive && shownMood == BuddyMood::Calm);
  equalParams(restParams(BuddyMood::Calm), EyeExpressionParams{0, 0, 18, 8, 0, 0, false, false});

  // Rollover-safe timing.
  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 200;
  reset(wrap, BuddyMood::Calm);
  setFaceMood(BuddyMood::Sleepy);
  idle(wrap);
  assert(moodMomentActive);
  idle(wrap + uint32_t{SLEEPY_MOOD_MOMENT_MS - 1});
  assert(moodMomentActive);
  idle(wrap + uint32_t{SLEEPY_MOOD_MOMENT_MS});
  assert(!moodMomentActive && shownMood == BuddyMood::Sleepy);
}

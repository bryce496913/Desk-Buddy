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

void prepare(FaceExpression expression, uint32_t now) {
  reactionExitTransitionActive = false;
  reactionEntryTransitionActive = false;
  blinkActive = false;
  lidAmount = 0;
  pupilX = pupilY = 0;
  renderFrame(now, BuddyCoreState::Awake, BuddyReaction::Idle);
  startFaceReaction(now, expression);
  renderFrame(now + 1750, BuddyCoreState::Awake, BuddyReaction::Generic);
}
int main() {
  for (FaceExpression expression : {FaceExpression::Happy, FaceExpression::Curious,
       FaceExpression::Annoyed, FaceExpression::Startled, FaceExpression::Suspicious,
       FaceExpression::Confused, FaceExpression::Daydreaming, FaceExpression::SideGlance,
       FaceExpression::Bored, FaceExpression::SleepyDrift, FaceExpression::SuspiciousGlance,
       FaceExpression::ExcitedScanning, FaceExpression::AnnoyedSquint}) {
    prepare(expression, 1000);
    const auto sourceLeft = renderedLeft;
    const auto sourceRight = renderedRight;
    assert(!isFaceReactionFinished(2799) && isFaceReactionFinished(2800));
    finishFaceReaction(2800);
    assert(reactionExitTransitionActive && !reactionEntryTransitionActive);
    equalParams(exitFromLeft, sourceLeft);
    equalParams(exitFromRight, sourceRight);
    equalParams(visualExpressionParams(2800, BuddyReaction::Idle, true), sourceLeft);
    equalParams(visualExpressionParams(2910, BuddyReaction::Idle, true),
                interpolateExpressionParams(sourceLeft, exitToLeft, 0.5f));
    equalParams(visualExpressionParams(2910, BuddyReaction::Idle, false),
                interpolateExpressionParams(sourceRight, exitToRight, 0.5f));
    // Expired idle deadlines cannot start any micro-action during exit.
    nextBlinkAt = nextDrowsyAt = nextLookAt = 2800;
    lastFrameAt = 2800;
    updateFaceRenderer(2910, BuddyCoreState::Awake, BuddyReaction::Idle);
    assert(!blinkActive && drowsyUntil == 2800);
    assert(pupilTargetX == 0 && pupilTargetY == 0);
    equalParams(renderedLeft, interpolateExpressionParams(sourceLeft, exitToLeft, 0.5f));
    const auto midpoint = renderedLeft;
    const auto midpointRight = renderedRight;
    startFaceReaction(2911, FaceExpression::Curious);
    assert(!reactionExitTransitionActive && reactionEntryTransitionActive);
    equalParams(entryFromLeft, midpoint);
    equalParams(entryFromRight, midpointRight);
    equalParams(visualExpressionParams(2911, BuddyReaction::Generic, true), midpoint);

    finishFaceReaction(3000);
    lastFrameAt = 3000;
    updateFaceRenderer(3220, BuddyCoreState::Awake, BuddyReaction::Idle);
    assert(!reactionExitTransitionActive);
    equalParams(renderedLeft, exitToLeft);
    equalParams(renderedRight, exitToRight);
    assert(pupilX == exitToLeft.pupilBiasX && pupilY == exitToLeft.pupilBiasY);
    assert(nextBlinkAt > 3220 && nextDrowsyAt > 3220 && nextLookAt > 3220);
    const uint32_t blinkDue = nextBlinkAt;
    updateFaceRenderer(blinkDue, BuddyCoreState::Awake, BuddyReaction::Idle);
    assert(blinkActive);
    nextDrowsyAt = nextLookAt = blinkDue + 1;
    updateFaceRenderer(blinkDue + 1, BuddyCoreState::Awake, BuddyReaction::Idle);
    assert(drowsyUntil != 3000 && pupilTargetX != 0);
  }
  prepare(FaceExpression::Startled, 100);
  finishFaceReaction(1900);
  enterSleepFace(1950);
  assert(!reactionExitTransitionActive && !reactionEntryTransitionActive);
  assert(backlightTarget == 40 && !blinkActive);
  updateFaceRenderer(2000, BuddyCoreState::Sleeping, BuddyReaction::Idle);
  assert(lidAmount > 0 && pupilTargetY == 10);
  wakeFace(2100);
  assert(!reactionExitTransitionActive && blinkActive);
  assert(blinkDuration == 260 && backlightTarget == 255);

  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 100;
  prepare(FaceExpression::Annoyed, wrap - 1800);
  finishFaceReaction(wrap);
  equalParams(visualExpressionParams(wrap + uint32_t{110}, BuddyReaction::Idle, true),
              interpolateExpressionParams(exitFromLeft, exitToLeft, 0.5f));
  lastFrameAt = wrap;
  updateFaceRenderer(wrap + uint32_t{219}, BuddyCoreState::Awake, BuddyReaction::Idle);
  assert(reactionExitTransitionActive);
  updateFaceRenderer(wrap + uint32_t{220}, BuddyCoreState::Awake, BuddyReaction::Idle);
  assert(!reactionExitTransitionActive);
  equalParams(visualExpressionParams(wrap + uint32_t{220}, BuddyReaction::Idle, true), exitToLeft);
  // Transition bookkeeping never bypasses the existing 50 ms render cadence.
  lastFrameAt = 5000;
  renderFrame(5000, BuddyCoreState::Awake, BuddyReaction::Idle);
  startFaceReaction(5000, FaceExpression::Happy);
  updateFaceRenderer(5049, BuddyCoreState::Awake, BuddyReaction::Generic);
  assert(lastFrameAt == 5000);
  updateFaceRenderer(5050, BuddyCoreState::Awake, BuddyReaction::Generic);
  assert(lastFrameAt == 5050);
}

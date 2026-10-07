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

void idleFrame(uint32_t now) {
  reactionEntryTransitionActive = false;
  activeExpression = FaceExpression::Normal;
  lidAmount = 0.6f;
  pupilX = 12;
  pupilY = 7;
  renderFrame(now, BuddyCoreState::Awake, BuddyReaction::Idle);
}
int main() {
  idleFrame(100);
  const auto source = renderedLeft;
  startFaceReaction(100, FaceExpression::Startled);
  equalParams(visualExpressionParams(100, BuddyReaction::Generic, true), source);
  assert(source.topLid == 0.6f && source.pupilBiasX == 12 && source.pupilBiasY == 7);
  const auto halfway = visualExpressionParams(190, BuddyReaction::Generic, true);
  assert(halfway.topLid > 0 && halfway.topLid < source.topLid);
  assert(halfway.irisRadius == 18 && halfway.pupilRadius == 6);
  assert(halfway.pupilBiasX == 6 && halfway.pupilBiasY == 4);
  assert(halfway.reactiveIris && halfway.showSpark);
  assert(!visualExpressionParams(189, BuddyReaction::Generic, true).showSpark);
  equalParams(visualExpressionParams(280, BuddyReaction::Generic, true),
              expressionParams(FaceExpression::Startled, true, 280));
  assert(!isFaceReactionFinished(1899) && isFaceReactionFinished(1900));

  renderFrame(190, BuddyCoreState::Awake, BuddyReaction::Generic);
  const auto intermediateLeft = renderedLeft;
  const auto intermediateRight = renderedRight;
  startFaceReaction(200, FaceExpression::Happy);
  equalParams(entryFromLeft, intermediateLeft);
  equalParams(entryFromRight, intermediateRight);
  equalParams(visualExpressionParams(200, BuddyReaction::Generic, true), intermediateLeft);
  equalParams(visualExpressionParams(380, BuddyReaction::Generic, true),
              expressionParams(FaceExpression::Happy, true, 380));

  for (FaceExpression expression : {FaceExpression::SleepyDrift, FaceExpression::ExcitedScanning}) {
    idleFrame(1000);
    startFaceReaction(1000, expression);
    const auto movingTarget = expressionParams(expression, true, 1090);
    equalParams(visualExpressionParams(1090, BuddyReaction::Generic, true),
                interpolateExpressionParams(entryFromLeft, movingTarget, 0.5f));
    equalParams(visualExpressionParams(1180, BuddyReaction::Generic, true),
                expressionParams(expression, true, 1180));
    assert(reactionStartedAt == 1000);
  }
  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 100;
  idleFrame(wrap);
  startFaceReaction(wrap, FaceExpression::Curious);
  const auto regularSource = entryFromLeft;
  equalParams(visualExpressionParams(wrap, BuddyReaction::Generic, true), regularSource);
  equalParams(visualExpressionParams(wrap + uint32_t{90}, BuddyReaction::Generic, true),
              interpolateExpressionParams(regularSource, expressionParams(FaceExpression::Curious, true, wrap + uint32_t{90}), 0.5f));
  updateFaceRenderer(wrap + uint32_t{180}, BuddyCoreState::Awake, BuddyReaction::Generic);
  assert(!reactionEntryTransitionActive);
  equalParams(renderedLeft, expressionParams(FaceExpression::Curious, true, wrap + uint32_t{180}));

  idleFrame(100);
  startFaceReaction(100, FaceExpression::SideGlance);
  const int firstDirection = expressionParams(FaceExpression::SideGlance, true, 100).pupilBiasX;
  assert(expressionParams(FaceExpression::SideGlance, true, 190).pupilBiasX == firstDirection);
  startFaceReaction(200, FaceExpression::SideGlance);
  assert(expressionParams(FaceExpression::SideGlance, true, 200).pupilBiasX == -firstDirection);
  nextBlinkAt = nextDrowsyAt = nextLookAt = 0;
  blinkActive = false;
  updateFaceRenderer(250, BuddyCoreState::Awake, BuddyReaction::Generic);
  assert(!blinkActive && drowsyUntil == 0);
  enterSleepFace(260);
  assert(!reactionEntryTransitionActive && backlightTarget == 40);
  wakeFace(270);
  assert(!reactionEntryTransitionActive && blinkDuration == 260 && backlightTarget == 255);
  startFaceReaction(300, FaceExpression::Happy);
  finishFaceReaction(400);
  assert(!reactionEntryTransitionActive && activeExpression == FaceExpression::Normal);
}

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
uint32_t presentationAt(uint32_t start, float progress) {
  return start + REACTION_ENTER_TRANSITION_MS + uint32_t(lroundf(progress * 1620));
}
int main() {
  constexpr uint32_t start = 1000;
  reactionStartedAt = start;
  for (FaceExpression expression : {FaceExpression::Happy, FaceExpression::Curious,
       FaceExpression::Annoyed, FaceExpression::Confused, FaceExpression::Startled}) {
    for (bool left : {false, true}) {
      for (uint32_t elapsed : {0u, 90u, 180u})
        equalParams(expressionParams(expression, left, start + elapsed),
                    baseExpressionParams(expression, left, start + elapsed));
    }
  }
  const auto happyBase = baseExpressionParams(FaceExpression::Happy, true, start);
  const auto happyMid = expressionParams(FaceExpression::Happy, true, presentationAt(start, 0.5f));
  assert(happyMid.pupilBiasY == happyBase.pupilBiasY - 3);
  near(happyMid.bottomLid, happyBase.bottomLid + 0.03f);
  equalParams(expressionParams(FaceExpression::Happy, true, start + 1800), happyBase);
  // Every sampled Happy frame stays inside the three-pixel budget.
  for (uint32_t elapsed = 0; elapsed <= 1800; elapsed += 10) {
    const auto p = expressionParams(FaceExpression::Happy, true, start + elapsed);
    assert(p.pupilBiasY <= happyBase.pupilBiasY && p.pupilBiasY >= happyBase.pupilBiasY - 3);
  }
  startFaceReaction(start, FaceExpression::Curious);
  const bool direction = curiousShiftRight;
  const auto curiousBase = baseExpressionParams(FaceExpression::Curious, true, start);
  equalParams(expressionParams(FaceExpression::Curious, true, presentationAt(start, 0.35f)), curiousBase);
  const int shift = direction ? 5 : -5;
  assert(expressionParams(FaceExpression::Curious, true, presentationAt(start, 0.6f)).pupilBiasX == curiousBase.pupilBiasX + shift);
  assert(expressionParams(FaceExpression::Curious, false, start + 1800).pupilBiasX == curiousBase.pupilBiasX + shift);
  assert(curiousShiftRight == direction);
  startFaceReaction(start + 2000, FaceExpression::Happy);
  assert(curiousShiftRight == direction);
  startFaceReaction(start + 4000, FaceExpression::Curious);
  assert(curiousShiftRight != direction);
  assert(expressionParams(FaceExpression::Curious, true, start + 5800).pupilBiasX == curiousBase.pupilBiasX - shift);
  reactionStartedAt = start;
  for (bool left : {false, true}) {
    const auto annoyedBase = baseExpressionParams(FaceExpression::Annoyed, left, start);
    const auto confusedBase = baseExpressionParams(FaceExpression::Confused, left, start);
    equalParams(expressionParams(FaceExpression::Confused, left, presentationAt(start, 0.3f)), confusedBase);
    const auto confusedEnd = expressionParams(FaceExpression::Confused, left, presentationAt(start, 0.65f));
    assert(confusedEnd.pupilBiasX == confusedBase.pupilBiasX + (left ? -3 : 3));
    assert(confusedEnd.topLid == confusedBase.topLid && confusedEnd.bottomLid == confusedBase.bottomLid);
    for (uint32_t elapsed = 0; elapsed <= 1800; elapsed += 10) {
      const auto annoyed = expressionParams(FaceExpression::Annoyed, left, start + elapsed);
      assert(annoyed.topLid >= annoyedBase.topLid && annoyed.topLid <= annoyedBase.topLid + 0.08001f);
      assert(annoyed.bottomLid >= annoyedBase.bottomLid && annoyed.bottomLid <= annoyedBase.bottomLid + 0.03001f);
      assert(annoyed.topLid < 1 && annoyed.bottomLid < 1);
      const auto confused = expressionParams(FaceExpression::Confused, left, start + elapsed);
      assert(std::abs(confused.pupilBiasX - confusedBase.pupilBiasX) <= 3);
      assert(confused.pupilBiasY == confusedBase.pupilBiasY);
    }
    const auto tightened = expressionParams(FaceExpression::Annoyed, left, presentationAt(start, 0.5f));
    near(tightened.topLid, annoyedBase.topLid + 0.08f);
    near(tightened.bottomLid, annoyedBase.bottomLid + 0.03f);
  }
  const auto startledStart = expressionParams(FaceExpression::Startled, true, start);
  assert(startledStart.topLid == 0 && startledStart.bottomLid == 0 && startledStart.pupilRadius == 4);
  bool sparkEnded = false;
  for (uint32_t elapsed = 0; elapsed <= 1800; elapsed += 10) {
    const auto p = expressionParams(FaceExpression::Startled, true, start + elapsed);
    assert(p.reactiveIris && p.pupilRadius >= 4 && p.pupilRadius <= 6);
    if (!p.showSpark) sparkEnded = true;
    if (sparkEnded) assert(!p.showSpark);
  }
  assert(expressionParams(FaceExpression::Startled, true, start + 827).showSpark);
  assert(!expressionParams(FaceExpression::Startled, true, start + 828).showSpark);
  const auto startledEnd = expressionParams(FaceExpression::Startled, true, start + 1800);
  near(startledEnd.topLid, 0.08f); near(startledEnd.bottomLid, 0.05f);
  assert(startledEnd.pupilRadius == 6 && startledEnd.irisRadius == 18 && startledEnd.reactiveIris && !startledEnd.showSpark);

  // All five animations preserve their parameters across clock rollover.
  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 100;
  for (FaceExpression expression : {FaceExpression::Happy, FaceExpression::Curious,
       FaceExpression::Annoyed, FaceExpression::Confused, FaceExpression::Startled}) {
    for (uint32_t elapsed : {0u, 180u, 500u, 990u, 1500u, 1800u}) {
      reactionStartedAt = start;
      const auto regular = expressionParams(expression, true, start + elapsed);
      reactionStartedAt = wrap;
      equalParams(expressionParams(expression, true, wrap + elapsed), regular);
    }
    reactionEntryTransitionActive = reactionExitTransitionActive = false;
    lidAmount = 0.4f; pupilX = 9; pupilY = 5;
    renderFrame(start, BuddyCoreState::Awake, BuddyReaction::Idle);
    const auto source = renderedLeft;
    startFaceReaction(start, expression);
    equalParams(visualExpressionParams(start, BuddyReaction::Generic, true), source);
    equalParams(visualExpressionParams(start + 90, BuddyReaction::Generic, true),
                interpolateExpressionParams(source, expressionParams(expression, true, start + 90), 0.5f));
    renderFrame(start + 1400, BuddyCoreState::Awake, BuddyReaction::Generic);
    const auto currentLeft = renderedLeft;
    const auto currentRight = renderedRight;
    startFaceReaction(start + 1401, FaceExpression::Suspicious);
    equalParams(entryFromLeft, currentLeft); equalParams(entryFromRight, currentRight);
    equalParams(visualExpressionParams(start + 1401, BuddyReaction::Generic, true), currentLeft);
    startFaceReaction(start + 2000, expression);
    renderFrame(start + 3750, BuddyCoreState::Awake, BuddyReaction::Generic);
    const auto endpoint = renderedLeft;
    finishFaceReaction(start + 3800);
    equalParams(exitFromLeft, endpoint);
    equalParams(visualExpressionParams(start + 3910, BuddyReaction::Idle, true),
                interpolateExpressionParams(endpoint, exitToLeft, 0.5f));
  }
}

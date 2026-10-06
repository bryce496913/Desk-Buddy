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

static_assert(static_cast<uint8_t>(FaceExpression::Confused) == 7);
static_assert(static_cast<uint8_t>(FaceExpression::Daydreaming) == 3);

void expectSame(const EyeExpressionParams& a, const EyeExpressionParams& b) {
  assert(a.topLid == b.topLid && a.bottomLid == b.bottomLid);
  assert(a.irisRadius == b.irisRadius && a.pupilRadius == b.pupilRadius);
  assert(a.pupilBiasX == b.pupilBiasX && a.pupilBiasY == b.pupilBiasY);
  assert(a.reactiveIris == b.reactiveIris && a.showSpark == b.showSpark);
}

int main() {
  const FaceExpression additions[] = {FaceExpression::SideGlance, FaceExpression::Bored,
      FaceExpression::SleepyDrift, FaceExpression::SuspiciousGlance,
      FaceExpression::ExcitedScanning, FaceExpression::AnnoyedSquint};
  for (FaceExpression expression : additions) {
    startFaceReaction(100, expression);
    for (bool left : {false, true}) {
      const auto params = expressionParams(expression, left, 100);
      assert(params.topLid >= 0 && params.topLid < 0.93f);
      assert(params.bottomLid >= 0 && params.bottomLid < 0.93f);
      assert(!params.showSpark && !params.reactiveIris);
      assert(params.irisRadius > 0 && params.pupilRadius > 0);
    }
    updateFaceRenderer(150, BuddyCoreState::Awake, BuddyReaction::Generic);
    assert(backlightTarget == 255);
    assert(!isFaceReactionFinished(1899));
    assert(isFaceReactionFinished(1900));
  }
  startFaceReaction(100, FaceExpression::SideGlance);
  const int direction = expressionParams(FaceExpression::SideGlance, true, 100).pupilBiasX;
  startFaceReaction(200, FaceExpression::SideGlance);
  assert(expressionParams(FaceExpression::SideGlance, true, 200).pupilBiasX == -direction);
  assert(expressionParams(FaceExpression::SideGlance, false, 200).pupilBiasX == -direction);
  startFaceReaction(300, FaceExpression::SideGlance);
  assert(expressionParams(FaceExpression::SideGlance, true, 300).pupilBiasX == direction);

  const auto bored = expressionParams(FaceExpression::Bored, true, 100);
  const auto daydream = expressionParams(FaceExpression::Daydreaming, true, 100);
  assert(bored.topLid > daydream.topLid && bored.pupilBiasY > 0 && daydream.pupilBiasY < 0);
  const auto glance = expressionParams(FaceExpression::SuspiciousGlance, false, 100);
  const auto suspicious = expressionParams(FaceExpression::Suspicious, false, 100);
  assert(glance.topLid > suspicious.topLid && glance.pupilBiasX < 0 && suspicious.pupilBiasX > 0);
  assert(expressionParams(FaceExpression::AnnoyedSquint, false, 100).topLid <
         expressionParams(FaceExpression::Annoyed, false, 100).topLid);

  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 500;
  for (FaceExpression expression : {FaceExpression::SleepyDrift, FaceExpression::ExcitedScanning}) {
    for (uint32_t elapsed : {0u, 450u, 900u, 1170u, 1500u, 1799u, 1800u}) {
      startFaceReaction(100, expression);
      const auto regular = expressionParams(expression, true, 100 + elapsed);
      startFaceReaction(wrap, expression);
      const auto wrapped = expressionParams(expression, true, wrap + elapsed);
      expectSame(regular, wrapped);
      assert(isFaceReactionFinished(wrap + elapsed) == (elapsed >= 1800));
    }
  }
  startFaceReaction(100, FaceExpression::SleepyDrift);
  const auto start = expressionParams(FaceExpression::SleepyDrift, true, 100);
  const auto middle = expressionParams(FaceExpression::SleepyDrift, true, 1270);
  const auto end = expressionParams(FaceExpression::SleepyDrift, true, 1900);
  assert(middle.topLid > start.topLid && middle.pupilBiasY > start.pupilBiasY);
  assert(end.topLid < middle.topLid && end.pupilBiasY < middle.pupilBiasY);
  startFaceReaction(100, FaceExpression::ExcitedScanning);
  assert(expressionParams(FaceExpression::ExcitedScanning, true, 100).pupilBiasX == -15);
  assert(expressionParams(FaceExpression::ExcitedScanning, true, 1288).pupilBiasX == 15);
  assert(expressionParams(FaceExpression::ExcitedScanning, true, 1900).pupilBiasX == 0);
  // Existing validated parameters remain unchanged by reaction timing plumbing.
  expectSame(expressionParams(FaceExpression::Happy, true, 100),
             EyeExpressionParams{0.10f, 0.34f, 18, 8, 3, -6, false, false});
  expectSame(expressionParams(FaceExpression::Annoyed, false, 100),
             EyeExpressionParams{0.60f, 0.0f, 18, 8, 3, 2, false, false});
}

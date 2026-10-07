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
int main() {
  const EyeExpressionParams from{0.2f, 0.4f, 10, 3, -9, 8, false, true};
  const EyeExpressionParams to{0.8f, 0.6f, 19, 8, 8, -9, true, false};
  equalParams(interpolateExpressionParams(from, to, 0), from);
  equalParams(interpolateExpressionParams(from, to, 1), to);
  equalParams(interpolateExpressionParams(from, to, -1), from);
  equalParams(interpolateExpressionParams(from, to, 2), to);
  const auto mid = interpolateExpressionParams(from, to, 0.5f);
  assert(std::fabs(mid.topLid - 0.5f) < 0.00001f);
  assert(std::fabs(mid.bottomLid - 0.5f) < 0.00001f);
  assert(mid.irisRadius == 15 && mid.pupilRadius == 6);
  assert(mid.pupilBiasX == -1 && mid.pupilBiasY == -1);  // Negative halves round away from zero.
  assert(mid.reactiveIris && !mid.showSpark);
  const auto before = interpolateExpressionParams(from, to, 0.499f);
  assert(!before.reactiveIris && before.showSpark);
  const auto after = interpolateExpressionParams(from, to, 0.501f);
  assert(after.reactiveIris && !after.showSpark);
  equalParams(interpolateExpressionParams(from, from, 0.5f), from);
  assert(smoothstep01(-1) == 0 && smoothstep01(0) == 0);
  assert(smoothstep01(1) == 1 && smoothstep01(2) == 1);
  assert(smoothstep01(0.5f) == 0.5f);
  assert(smoothstep01(0.25f) == 0.15625f);
  assert(smoothstep01(0.75f) == 0.84375f);
  const EyeExpressionParams invalid{-2, 3, -10, 0, -3, 3, false, false};
  const EyeExpressionParams invalidTo{3, -2, 0, -10, 3, -3, true, true};
  for (float progress : {-1.0f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f}) {
    const auto params = interpolateExpressionParams(invalid, invalidTo, progress);
    assert(params.topLid >= 0 && params.topLid <= 1);
    assert(params.bottomLid >= 0 && params.bottomLid <= 1);
    assert(params.irisRadius >= 1 && params.pupilRadius >= 1);
  }
  // Every existing valid endpoint is copied exactly, including animated snapshots.
  for (uint8_t value = 0; value <= static_cast<uint8_t>(FaceExpression::AnnoyedSquint); ++value) {
    const auto expression = static_cast<FaceExpression>(value);
    startFaceReaction(100, expression);
    for (bool left : {false, true}) {
      for (uint32_t now : {100u, 550u, 1270u, 1899u}) {
        const auto endpoint = expressionParams(expression, left, now);
        equalParams(interpolateExpressionParams(from, endpoint, 1), endpoint);
        equalParams(interpolateExpressionParams(endpoint, to, 0), endpoint);
        const auto blend = interpolateExpressionParams(from, endpoint, smoothstep01(0.25f));
        assert(blend.irisRadius >= 1 && blend.pupilRadius >= 1);
      }
    }
  }
}

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
int main() {
  for (uint32_t start : {1000u, std::numeric_limits<uint32_t>::max() - 100u}) {
    reactionStartedAt = start;
    near(reactionProgress01(start), 0);
    near(reactionProgress01(start + 900u), 0.5f);
    near(reactionProgress01(start + 1800u), 1);
    near(reactionProgress01(start + 5000u), 1);
    for (uint32_t elapsed : {0u, 90u, 179u, 180u})
      near(reactionPresentationProgress01(start + elapsed), 0);
    near(reactionPresentationProgress01(start + 181u), 1.0f / 1620);
    near(reactionPresentationProgress01(start + 990u), 0.5f);
    near(reactionPresentationProgress01(start + 1800u), 1);
    near(reactionPresentationProgress01(start + 5000u), 1);
  }
  near(smoothPulse(-1), 0);
  near(smoothPulse(0), 0);
  near(smoothPulse(0.25f), 0.5f);
  near(smoothPulse(0.5f), 1);
  near(smoothPulse(0.75f), 0.5f);
  near(smoothPulse(1), 0);
  near(smoothPulse(2), 0);
  for (int step = 0; step <= 100; ++step) {
    const float p = float(step) / 100;
    assert(smoothPulse(p) >= 0 && smoothPulse(p) <= 1);
    near(smoothPulse(p), smoothPulse(1 - p));
  }
  // All existing targets pass through the new stage without any visible retuning.
  reactionStartedAt = 1000;
  lidAmount = 0.37f;
  for (uint8_t value = 0; value <= static_cast<uint8_t>(FaceExpression::AnnoyedSquint); ++value) {
    const auto expression = static_cast<FaceExpression>(value);
    for (bool left : {false, true}) {
      for (uint32_t elapsed : {0u, 90u, 180u, 900u, 1188u, 1800u, 2200u}) {
        const uint32_t now = 1000 + elapsed;
        const auto base = baseExpressionParams(expression, left, now);
        equalParams(applyReactionMicroAnimation(expression, left, now, base), base);
        equalParams(expressionParams(expression, left, now), base);
        // Independently check the original autonomous motion formulas.
        const float p = std::clamp(float(elapsed) / 1800, 0.0f, 1.0f);
        if (expression == FaceExpression::SleepyDrift) {
          const float drift = p < 0.65f ? p / 0.65f : 1 - 0.65f * ((p - 0.65f) / 0.35f);
          near(base.topLid, 0.48f + drift * 0.22f);
          assert(base.pupilBiasX == int(drift * 5) && base.pupilBiasY == int(drift * 13));
        }
        if (expression == FaceExpression::ExcitedScanning) {
          const float scan = p < 0.66f ? -15 + 30 * (p / 0.66f)
              : 15 + (0 - 15) * ((p - 0.66f) / 0.34f);
          assert(base.pupilBiasX == int(scan));
        }
      }
    }
  }
  // Idle retains its own lid/pupil systems; reaction time does not affect Normal.
  reactionExitTransitionActive = false;
  pupilX = 12; pupilY = -7;
  const auto idle = visualExpressionParams(1000, BuddyReaction::Idle, true);
  equalParams(visualExpressionParams(2400, BuddyReaction::Idle, true), idle);
  assert(idle.topLid == 0.37f && idle.pupilBiasX == 12 && idle.pupilBiasY == -7);
  renderFrame(1000, BuddyCoreState::Awake, BuddyReaction::Idle);
  startFaceReaction(1000, FaceExpression::SleepyDrift);
  equalParams(visualExpressionParams(1090, BuddyReaction::Generic, true),
              interpolateExpressionParams(entryFromLeft, expressionParams(FaceExpression::SleepyDrift, true, 1090), 0.5f));
  renderFrame(2750, BuddyCoreState::Awake, BuddyReaction::Generic);
  const auto endpoint = renderedLeft;
  finishFaceReaction(2800);
  equalParams(exitFromLeft, endpoint);
  equalParams(visualExpressionParams(2910, BuddyReaction::Idle, true),
              interpolateExpressionParams(endpoint, exitToLeft, 0.5f));
  equalParams(exitFromLeft, endpoint);
}

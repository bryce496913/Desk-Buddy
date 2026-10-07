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
#include "SoundEngine.h"
#include "Diagnostics.h"
HardwareSerial Serial;
bool audioActive = false;
int audioStops = 0;
void startReactionSound(uint32_t, ReactionSound sound, BuddyMood) { audioActive = sound != ReactionSound::None; }
bool startDiagnosticReactionSound(uint32_t now, ReactionSound sound, uint8_t, uint8_t& selected) {
  selected = 0; startReactionSound(now, sound, BuddyMood::Calm); return true;
}
void stopReactionSound() { audioActive = false; ++audioStops; }
bool isSoundEngineActive() { return audioActive; }
void playSleepSound() { audioActive = false; }
void playWakeSound() {}
void ignoreSoundSensorAfterWake() {}
void command(const char* text, uint32_t now) {
  for (const char* p = text; *p; ++p) { Serial.push(*p); updateDiagnostics(now); }
}
void reset(uint32_t now) {
  audioActive = false;
  reactionEntryTransitionActive = reactionExitTransitionActive = false;
  hasRenderedEyes = false; blinkActive = false; lidAmount = 0;
  pupilX = pupilY = pupilTargetX = pupilTargetY = 0;
  beginBehaviorEngine(now);
  renderFrame(now, BuddyCoreState::Awake, BuddyReaction::Idle);
  lastFrameAt = now;
}
void frame(uint32_t now) {
  updateFaceRenderer(now, getBuddyCoreState(), getBuddyReaction());
}
void checkDraw(const EyeExpressionParams& p) {
  eyeCanvas.circleCount = 0;
  drawEye(eyeCanvas, 40, 58, eyeW, eyeH, p);
  assert(eyeCanvas.circleCount >= 2);
  const auto iris = eyeCanvas.circles[0];
  const auto pupil = eyeCanvas.circles[1];
  assert(iris.x == pupil.x && iris.y == pupil.y);
  assert(std::abs(iris.x - 40) <= 17 && std::abs(iris.y - 58) <= 24);
  assert(iris.x - iris.radius >= 40 - eyeW / 2);
  assert(iris.x + iris.radius <= 40 + eyeW / 2);
  assert(iris.y - iris.radius >= 58 - eyeH / 2);
  assert(iris.y + iris.radius <= 58 + eyeH / 2);
}
int main() {
  static_assert(HAPPY_BOUNCE_PX <= 3 && CURIOUS_SHIFT_PX <= 6 && CONFUSED_DIVERGE_PX <= 4);
  static_assert(ANNOYED_TOP_TIGHTEN <= 0.08f && ANNOYED_BOTTOM_TIGHTEN <= 0.03f);
  static_assert(STARTLED_PUPIL_GROWTH_PX <= 2);
  static_assert(EYE_REGION_W == 80 && EYE_REGION_H == 117);
  static_assert(EFFECT_REGION_W == 12 && EFFECT_REGION_H == 22);
  static_assert(REACTION_DURATION_MS == 1800 && REACTION_ENTER_TRANSITION_MS == 180 && REACTION_EXIT_TRANSITION_MS == 220);
  // Real diagnostics -> engine -> renderer replacement matrix, including Hold policy.
  struct Replacement { const char* first; const char* second; FaceExpression target; };
  for (const auto& pair : {Replacement{"1", "4", FaceExpression::Startled},
       Replacement{"4", "2", FaceExpression::Curious}, Replacement{"3", "6", FaceExpression::Confused},
       Replacement{"2", "1", FaceExpression::Happy}, Replacement{"6", nullptr, FaceExpression::Happy}}) {
    reset(1000); command(pair.first, 1000);
    for (uint32_t now = 1050; now <= 2000; now += 50) frame(now);
    const auto left = renderedLeft; const auto right = renderedRight;
    if (pair.second) command(pair.second, 2001);
    else processBuddyEvent(BuddyEvent::TouchHold, 2001);
    assert(activeExpression == pair.target && getBuddyReaction() == BuddyReaction::Generic);
    equalParams(entryFromLeft, left); equalParams(entryFromRight, right);
    equalParams(visualExpressionParams(2001, BuddyReaction::Generic, true), left);
    frame(2049); equalParams(renderedLeft, left); // No redraw/Normal flash before 50 ms.
    frame(2050); assert(lastFrameAt == 2050);
  }
  for (const char* text : {"1", "2", "3", "4", "6"}) {
    reset(1000); command(text, 1000);
    for (uint32_t now = 1050; now <= 2750; now += 50) frame(now);
    const auto left = renderedLeft; const auto right = renderedRight;
    updateBehaviorEngine(2799); assert(getBuddyReaction() == BuddyReaction::Generic);
    updateBehaviorEngine(2800); assert(getBuddyReaction() == BuddyReaction::Idle);
    assert(reactionExitTransitionActive);
    equalParams(exitFromLeft, left); equalParams(exitFromRight, right);
    frame(2800); equalParams(renderedLeft, left);
    frame(2910); equalParams(renderedLeft, interpolateExpressionParams(left, exitToLeft, 0.5f));
    frame(3020); assert(!reactionExitTransitionActive); equalParams(renderedLeft, exitToLeft);

    reset(1000); command(text, 1000); frame(2000);
    const auto early = renderedLeft; const int stops = audioStops;
    assert(audioActive); command("0", 2001);
    assert(!audioActive && audioStops == stops + 1 && getBuddyReaction() == BuddyReaction::Idle);
    equalParams(exitFromLeft, early);
    frame(2111); equalParams(renderedLeft, interpolateExpressionParams(early, exitToLeft, 0.5f));
    equalParams(exitFromLeft, early);

    reset(1000); command(text, 1000); frame(2000);
    processBuddyEvent(BuddyEvent::ButtonPressed, 2001);
    assert(getBuddyCoreState() == BuddyCoreState::Sleeping && getBuddyReaction() == BuddyReaction::Idle);
    assert(!reactionEntryTransitionActive && !reactionExitTransitionActive);
    assert(activeExpression == FaceExpression::Normal && !blinkActive && backlightTarget == 40);
    frame(2050); assert(renderedLeft.topLid > 0 && !renderedLeft.showSpark);
    processBuddyEvent(BuddyEvent::ButtonPressed, 2100);
    assert(getBuddyCoreState() == BuddyCoreState::Awake && blinkActive && blinkDuration == 260 && backlightTarget == 255);
  }
  // Direction bits remain independent even with interrupted Curious reactions.
  reset(1000); command("2", 1000);
  const bool curious = curiousShiftRight; const bool side = sideGlanceRight;
  for (uint32_t now = 1050; now <= 2500; now += 50) { frame(now); assert(curiousShiftRight == curious); }
  command("a3", 2501); assert(sideGlanceRight != side && curiousShiftRight == curious);
  command("2", 2600); assert(curiousShiftRight != curious && sideGlanceRight != side);
  command("a3", 2700); assert(sideGlanceRight == side && curiousShiftRight != curious);

  for (const char* text : {"a8", "a4"}) {
    reset(1000); command(text, 1000); frame(2000);
    const auto source = renderedLeft;
    processBuddyEvent(text[1] == '8' ? BuddyEvent::TouchTap : BuddyEvent::SoundDetected, 2001);
    assert(getBuddyReaction() == BuddyReaction::Generic);
    equalParams(entryFromLeft, source);
    equalParams(visualExpressionParams(2001, BuddyReaction::Generic, true), source);
  }
  reset(1000); command("1", 1000); frame(2000);
  const uint32_t clock = reactionStartedAt;
  processBuddyEvent(BuddyEvent::IdleTimeout, 2001);
  assert(activeExpression == FaceExpression::Happy && reactionStartedAt == clock);

  // Actual 20 FPS output: monotonic one-way motion, one Happy peak, no spark flicker.
  constexpr uint32_t wrap = std::numeric_limits<uint32_t>::max() - 100;
  for (FaceExpression expression : {FaceExpression::Happy, FaceExpression::Curious,
       FaceExpression::Annoyed, FaceExpression::Confused, FaceExpression::Startled}) {
    for (bool left : {false, true}) {
      EyeExpressionParams previous{}; bool first = true; bool sparkEnded = false;
      for (uint32_t elapsed = 0; elapsed <= 1800; elapsed += 50) {
        reactionStartedAt = 1000;
        const auto p = expressionParams(expression, left, 1000 + elapsed);
        reactionStartedAt = wrap;
        equalParams(expressionParams(expression, left, wrap + elapsed), p);
        checkDraw(p);
        if (!first && elapsed > 180) {
          if (expression == FaceExpression::Happy) {
            if (elapsed <= 990) assert(p.pupilBiasY <= previous.pupilBiasY);
            else if (elapsed >= 1050) assert(p.pupilBiasY >= previous.pupilBiasY);
            assert(std::abs(p.pupilBiasY - previous.pupilBiasY) <= 1);
          }
          if (expression == FaceExpression::Curious)
            assert(curiousShiftRight ? p.pupilBiasX >= previous.pupilBiasX : p.pupilBiasX <= previous.pupilBiasX);
          if (expression == FaceExpression::Confused)
            assert(left ? p.pupilBiasX <= previous.pupilBiasX : p.pupilBiasX >= previous.pupilBiasX);
          if (expression == FaceExpression::Annoyed || expression == FaceExpression::Startled)
            assert(p.topLid >= previous.topLid && p.bottomLid >= previous.bottomLid);
          if (expression == FaceExpression::Startled)
            assert(p.pupilRadius >= previous.pupilRadius && p.irisRadius >= previous.irisRadius);
        }
        if (expression == FaceExpression::Startled) {
          if (!p.showSpark) sparkEnded = true;
          if (sparkEnded) assert(!p.showSpark);
        }
        previous = p; first = false;
      }
    }
  }
  // Ambient offset is captured once; drawing never adds it again during entry.
  reset(1000); pupilX = 17; pupilY = 24;
  renderFrame(1000, BuddyCoreState::Awake, BuddyReaction::Idle);
  command("6", 1000);
  for (uint32_t elapsed : {0u, 50u, 100u, 150u, 180u})
    checkDraw(visualExpressionParams(1000 + elapsed, BuddyReaction::Generic, true));
  assert(pupilX == 0 && pupilY == 0);
}

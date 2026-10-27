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
// randomHigh selects each range's top value, e.g. to force a double blink.
bool randomHigh = false;
long random(long low, long high) { return randomHigh ? high - 1 : low; }
long random(long high) { return randomHigh ? high - 1 : 0; }
// Include the actual renderer so private procedural geometry can be inspected.
#include "../FaceRenderer.cpp"

void near(float actual, float expected, float tolerance = 0.00001f) {
  assert(std::fabs(actual - expected) < tolerance);
}
void reset(uint32_t now, BuddyMood mood = BuddyMood::Calm) {
  randomHigh = false;
  reactionExitTransitionActive = reactionEntryTransitionActive = false;
  moodMomentActive = snapMoodOnNextUpdate = false;
  wakeStretchActive = fallingAsleep = doubleBlinkQueued = false;
  hasRenderedEyes = blinkActive = false;
  lidAmount = lidEased = 0;
  pupilX = pupilY = pupilTargetX = pupilTargetY = 0;
  faceMood = shownMood = momentMood = mood;
  restPose = moodRestPose(mood);
  activeExpression = FaceExpression::Normal;
  reactionDurationMs = REACTION_DURATION_MS;
  reactionIntensity = 1.0f;
  idleSettledAt = now - 10000;
  nextBlinkAt = nextDrowsyAt = nextLookAt = now + 60000;
  drowsyUntil = now;
  backlightTarget = 255;
  backlightLevel = 255;
  backlightCurrent = 255;
  hasUpdated = true;
  lastUpdateAt = now;
  lastFrameAt = now;
}
void idle(uint32_t now) {
  updateFaceRenderer(now, BuddyCoreState::Awake, BuddyReaction::Idle);
}
void asleep(uint32_t now) {
  updateFaceRenderer(now, BuddyCoreState::Sleeping, BuddyReaction::Idle);
}
// Iris stays inside the eye wherever the pupil can be drawn.
void checkDrawable(const EyeExpressionParams& p) {
  assert(p.topLid >= 0 && p.topLid <= 1 && p.bottomLid >= 0 && p.bottomLid <= 1);
  assert(p.irisRadius >= 1 && p.pupilRadius >= 1 && p.pupilRadius < p.irisRadius);
  assert(17 + p.irisRadius <= eyeW / 2 && 24 + p.irisRadius <= eyeH / 2);
}

int main() {
  // 1. Easing is frame-rate independent: 1 ms and 25 ms steps land together.
  float coarsePupil = 0, finePupil = 0, coarseLight = 0, fineLight = 0;
  for (uint32_t step : {1u, 25u}) {
    reset(1000);
    pupilTargetX = 16;
    backlightTarget = 40;
    for (uint32_t now = 1000 + step; now <= 1300; now += step) idle(now);
    (step == 1 ? finePupil : coarsePupil) = pupilX;
    (step == 1 ? fineLight : coarseLight) = backlightLevel;
  }
  near(finePupil, coarsePupil, 0.01f);
  near(finePupil, 16.0f * (1.0f - expf(-300.0f / PUPIL_IDLE_TAU_MS)), 0.01f);
  near(fineLight, coarseLight, 0.1f);
  assert(fineLight > 40 && fineLight < 255);  // Dims gradually, not instantly.
  // A first update or a long stall snaps rather than overshooting.
  reset(2000);
  pupilTargetX = 10;
  idle(2000 + 5000);
  near(pupilX, 10.0f * (1.0f - expf(-float(MAX_EASE_STEP_MS) / PUPIL_IDLE_TAU_MS)), 0.01f);

  // Blinks are layered unsmoothed, so a blink still fully closes at its peak.
  reset(3000);
  nextBlinkAt = 3000;
  idle(3000);
  assert(blinkActive && blinkDuration == 140 && !doubleBlinkQueued);
  assert(nextBlinkAt == 3000 + 140 + 1800);
  idle(3070);
  near(lidAmount, 1.0f, 0.001f);

  // 6c. Occasional double blink: a quick second blink, then the normal cadence.
  reset(4000);
  randomHigh = true;
  nextBlinkAt = 4000;
  idle(4000);
  assert(blinkActive && doubleBlinkQueued && blinkDuration == 219);
  assert(nextBlinkAt == 4000 + 219 + DOUBLE_BLINK_GAP_MS);
  idle(4219);
  assert(!blinkActive);
  idle(4000 + 219 + DOUBLE_BLINK_GAP_MS);
  assert(blinkActive && !doubleBlinkQueued);
  assert(nextBlinkAt == 4000 + 219 + DOUBLE_BLINK_GAP_MS + 219 + 4199);

  // 4. Reaction length and size follow mood, plus jitter; Calm with no jitter is V2.
  struct Variation { BuddyMood mood; bool high; uint32_t duration; float intensity; };
  for (const auto& v : {Variation{BuddyMood::Calm, false, 1800, 1.0f},
                        Variation{BuddyMood::Calm, true, 2000, 1.15f},
                        Variation{BuddyMood::Engaged, false, 1620, 1.25f},
                        Variation{BuddyMood::Engaged, true, 1820, 1.25f * 1.15f},
                        Variation{BuddyMood::Sleepy, false, 2070, 0.70f},
                        Variation{BuddyMood::Grumpy, false, 1800, 1.0f}}) {
    reset(5000, v.mood);
    randomHigh = v.high;
    startFaceReaction(5000, FaceExpression::Happy);
    assert(reactionDurationMs == v.duration);
    near(reactionIntensity, v.intensity);
    assert(!isFaceReactionFinished(5000 + v.duration - 1) && isFaceReactionFinished(5000 + v.duration));
    near(reactionProgress01(5000 + v.duration / 2), 0.5f, 0.001f);
  }
  // Even the largest variation keeps every expression drawable.
  assert(reactionIntensity <= MAX_REACTION_INTENSITY + 0.0001f);
  for (FaceExpression expression : {FaceExpression::Happy, FaceExpression::Curious,
       FaceExpression::Annoyed, FaceExpression::Startled, FaceExpression::Confused,
       FaceExpression::Petted, FaceExpression::ContentBlink}) {
    reactionStartedAt = 5000;
    for (uint32_t t = 0; t <= reactionDurationMs; t += 25) {
      checkDrawable(expressionParams(expression, true, 5000 + t));
      checkDrawable(expressionParams(expression, false, 5000 + t));
    }
  }
  // Calm intensity reproduces the V2 Happy bounce exactly.
  reset(6000);
  startFaceReaction(6000, FaceExpression::Happy);
  assert(expressionParams(FaceExpression::Happy, true, 6000 + 990).pupilBiasY == -6 - HAPPY_BOUNCE_PX);
  reset(6000, BuddyMood::Sleepy);
  startFaceReaction(6000, FaceExpression::Happy);
  assert(expressionParams(FaceExpression::Happy, true, 6000 + 1125).pupilBiasY == -6 - 2);

  // 2. Petted sways on the wall clock, so a new purr cycle does not jump.
  reset(7000);
  reactionStartedAt = 7000;
  const auto pettedA = expressionParams(FaceExpression::Petted, true, 7600);
  reactionStartedAt = 7590;
  const auto pettedB = expressionParams(FaceExpression::Petted, true, 7600);
  assert(pettedA.pupilBiasX == pettedB.pupilBiasX && pettedA.topLid == pettedB.topLid);
  assert(expressionParams(FaceExpression::Petted, true, 7600).pupilBiasX == 3 + PETTED_SWAY_PX);
  // ContentBlink closes the top lid near the middle and reopens.
  reactionStartedAt = 7000;
  near(expressionParams(FaceExpression::ContentBlink, true, 7000).topLid, 0.10f);
  const float mid = 7000 + REACTION_ENTER_TRANSITION_MS + 0.325f * (1800 - REACTION_ENTER_TRANSITION_MS);
  near(expressionParams(FaceExpression::ContentBlink, true, uint32_t(mid)).topLid, 1.0f, 0.01f);
  near(expressionParams(FaceExpression::ContentBlink, true, 8800).topLid, 0.10f);

  // 5. Iris tint: Calm keeps the V2 colour; other moods tint; moments glide.
  reset(9000);
  renderFrame(9000, BuddyCoreState::Awake, BuddyReaction::Idle);
  assert(currentIrisColor == COL_IRIS);
  for (BuddyMood mood : {BuddyMood::Engaged, BuddyMood::Grumpy, BuddyMood::Sleepy}) {
    reset(9000, mood);
    renderFrame(9000, BuddyCoreState::Awake, BuddyReaction::Idle);
    assert(currentIrisColor != COL_IRIS);
  }
  reset(9000);
  setFaceMood(BuddyMood::Grumpy);
  idle(9000);
  assert(moodMomentActive);
  const uint16_t grumpy = rgb565(84, 135, 185);
  assert(moodIrisColor(9000) == COL_IRIS);
  assert(moodIrisColor(9450) != COL_IRIS && moodIrisColor(9450) != grumpy);
  assert(moodIrisColor(9900) == grumpy);

  // 3. Falling asleep: droop, a brief fight, then fully closed with a nod.
  reset(10000);
  enterSleepFace(10000);
  assert(fallingAsleep && backlightTarget == SLEEP_BACKLIGHT);
  float previous = 0, peakBeforeResist = 0, dip = 1;
  bool rose = false, dipped = false;
  int maxNod = 0;
  for (uint32_t t = 25; t < FALL_ASLEEP_MS; t += 25) {
    asleep(10000 + t);
    const float p = float(t) / FALL_ASLEEP_MS;
    if (p < 0.45f) peakBeforeResist = std::max(peakBeforeResist, lidAmount);
    if (p > 0.45f && p < 0.62f && lidAmount < previous) { dipped = true; dip = std::min(dip, lidAmount); }
    if (dipped && lidAmount > previous) rose = true;
    maxNod = std::max(maxNod, sleepNodOffsetPx(10000 + t));
    previous = lidAmount;
  }
  assert(peakBeforeResist > 0.6f && dipped && dip < peakBeforeResist - 0.1f && rose);
  assert(maxNod == SLEEP_NOD_PX && lidAmount < 1.0f && fallingAsleep);
  asleep(10000 + FALL_ASLEEP_MS);
  assert(!fallingAsleep && lidAmount == 1.0f && sleepNodOffsetPx(10000 + FALL_ASLEEP_MS) == 0);
  // 6a. Breathing backlight while asleep stays in a soft band.
  int lowest = 255, highest = 0;
  for (uint32_t t = 0; t <= SLEEP_BREATH_PERIOD_MS; t += 50) {
    asleep(10000 + FALL_ASLEEP_MS + t);
    lowest = std::min(lowest, backlightTarget);
    highest = std::max(highest, backlightTarget);
  }
  assert(lowest == SLEEP_BACKLIGHT && highest == SLEEP_BACKLIGHT + SLEEP_BREATH_DEPTH);
  // A wake cancels the fall-asleep script immediately.
  reset(12000);
  enterSleepFace(12000);
  asleep(12500);
  wakeFace(12600);
  assert(!fallingAsleep && backlightTarget == 255);

  // 3. Wake stretch: eyes widen, look left, look right, then settle into mood.
  reset(20000, BuddyMood::Sleepy);
  wakeFace(20000);
  idle(20000);
  assert(wakeStretchActive);
  nextBlinkAt = nextLookAt = nextDrowsyAt = 20001;  // Expired, but suppressed.
  const auto rest = expressionParams(FaceExpression::Normal, true, 20000);
  idle(20001);
  assert(!blinkActive || blinkDuration == 260);
  lidAmount = 0;
  const auto wide = expressionParams(FaceExpression::Normal, true, 20000 + WAKE_STRETCH_MS * 37 / 100);
  assert(wide.topLid < 0.01f && wide.irisRadius > rest.irisRadius && wide.pupilBiasY < 4);
  assert(expressionParams(FaceExpression::Normal, true, 20000 + WAKE_STRETCH_MS * 45 / 100).pupilBiasX < -5);
  assert(expressionParams(FaceExpression::Normal, true, 20000 + WAKE_STRETCH_MS * 70 / 100).pupilBiasX > 5);
  for (uint32_t t = 0; t <= WAKE_STRETCH_MS; t += 25) {
    checkDrawable(expressionParams(FaceExpression::Normal, true, 20000 + t));
  }
  idle(20000 + WAKE_STRETCH_MS - 1);
  assert(wakeStretchActive && pupilTargetX == 0);
  idle(20000 + WAKE_STRETCH_MS);
  assert(!wakeStretchActive && idleSettledAt == 20000 + WAKE_STRETCH_MS);
  assert(nextBlinkAt > 20000 + WAKE_STRETCH_MS);
  lidAmount = 0;
  const auto settled = expressionParams(FaceExpression::Normal, true, 20000 + WAKE_STRETCH_MS);
  near(settled.topLid, moodRestPose(BuddyMood::Sleepy).topLid);
  // A reaction during the stretch takes over cleanly.
  reset(30000);
  wakeFace(30000);
  idle(30000);
  startFaceReaction(30400, FaceExpression::Happy);
  assert(!wakeStretchActive);
}

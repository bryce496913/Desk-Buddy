#include "FaceRenderer.h"

#include <Arduino.h>
#include <math.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Config.h"

namespace {
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 eyeCanvas(EYE_REGION_W, EYE_REGION_H);
GFXcanvas16 effectCanvas(EFFECT_REGION_W, EFFECT_REGION_H);

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
const uint16_t COL_BG_TOP = rgb565(6, 10, 22);
const uint16_t COL_BG_BOTTOM = rgb565(10, 18, 38);
const uint16_t COL_PANEL = rgb565(18, 28, 56);
const uint16_t COL_EYE_WHITE = rgb565(232, 240, 255);
const uint16_t COL_EYE_LINE = rgb565(95, 180, 255);
const uint16_t COL_IRIS = rgb565(64, 170, 255);
const uint16_t COL_IRIS_REACT = rgb565(70, 220, 255);
const uint16_t COL_PUPIL = rgb565(8, 12, 20);
const uint16_t COL_HILITE = rgb565(255, 255, 255);
const uint16_t COL_SLEEP = rgb565(140, 210, 255);
const uint16_t COL_ACCENT = rgb565(40, 90, 180);

float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
float lerpf(float a, float b, float t) { return a + (b - a) * t; }

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

const int leftEyeX = 86;
const int rightEyeX = 194;
const int eyeY = 120;
const int eyeW = 78;
const int eyeH = 96;
const int eyeCorner = 28;
float pupilX = 0.0f;
float pupilY = 0.0f;
float pupilTargetX = 0.0f;
float pupilTargetY = 0.0f;
float lidAmount = 0.0f;
bool blinkActive = false;
uint32_t blinkStart = 0;
uint16_t blinkDuration = 180;
uint32_t nextBlinkAt = 0;
uint32_t nextLookAt = 0;
uint32_t drowsyUntil = 0;
uint32_t nextDrowsyAt = 0;
uint32_t reactUntil = 0;
uint32_t reactionStartedAt = 0;
constexpr uint32_t REACTION_DURATION_MS = 1800;
constexpr uint32_t REACTION_ENTER_TRANSITION_MS = 180;
constexpr uint32_t REACTION_EXIT_TRANSITION_MS = 220;
bool reactionExitTransitionActive = false;
uint32_t reactionExitStartedAt = 0;
bool reactionEntryTransitionActive = false;
uint32_t reactionEntryStartedAt = 0;
bool sideGlanceRight = false;
bool curiousShiftRight = false;
// Maximum extra motion relative to base geometry; all integer motion uses lroundf.
constexpr int HAPPY_BOUNCE_PX = 3;
constexpr float HAPPY_LID_LIFT = 0.03f;
constexpr int CURIOUS_SHIFT_PX = 5;
constexpr float ANNOYED_TOP_TIGHTEN = 0.08f;
constexpr float ANNOYED_BOTTOM_TIGHTEN = 0.03f;
constexpr int CONFUSED_DIVERGE_PX = 3;
constexpr float STARTLED_TOP_SETTLE = 0.08f;
constexpr float STARTLED_BOTTOM_SETTLE = 0.05f;
constexpr int STARTLED_PUPIL_GROWTH_PX = 2;
constexpr int STARTLED_IRIS_GROWTH_PX = 1;
constexpr float STARTLED_SPARK_END_PROGRESS = 0.40f;
// Mood shapes how long and how big each reaction plays; a little per-reaction
// jitter keeps repeats from looking identical. Calm with no jitter is V2 exactly.
constexpr uint32_t REACTION_JITTER_MS = 200;
constexpr long REACTION_INTENSITY_JITTER_PERCENT = 15;
constexpr float MAX_REACTION_INTENSITY = 1.25f * 1.15f;
uint32_t reactionDurationMs = REACTION_DURATION_MS;
float reactionIntensity = 1.0f;
FaceExpression activeExpression = FaceExpression::Normal;
uint32_t lastFrameAt = 0;
int backlightCurrent = 255;
int backlightTarget = 255;
// Easing is time-based so motion feels the same whatever the loop rate.
// Each tau is the time to close about 63% of the remaining gap.
constexpr float LID_EASE_TAU_MS = 45.0f;
constexpr float PUPIL_IDLE_TAU_MS = 110.0f;
constexpr float PUPIL_REACTION_TAU_MS = 40.0f;
constexpr float PUPIL_SLEEP_TAU_MS = 400.0f;
constexpr float BACKLIGHT_TAU_MS = 350.0f;
constexpr uint32_t MAX_EASE_STEP_MS = 1000;
uint32_t lastUpdateAt = 0;
bool hasUpdated = false;
float lidEased = 0.0f;  // Drowsy/sleep lid; blinks layer on top unsmoothed.
float backlightLevel = 255.0f;
// Falling asleep: lids droop, fight back once, then close with a small nod.
constexpr uint32_t FALL_ASLEEP_MS = 1800;
constexpr int SLEEP_NOD_PX = 6;
constexpr int SLEEP_BACKLIGHT = 40;
constexpr int SLEEP_BREATH_DEPTH = 14;
constexpr uint32_t SLEEP_BREATH_PERIOD_MS = 4500;
bool fallingAsleep = false;
uint32_t fallAsleepStartedAt = 0;
float fallAsleepFromLid = 0.0f;
uint32_t sleepBreathStartedAt = 0;
// Waking: the wake blink, then a stretch (eyes wide, look left, look right).
constexpr uint32_t WAKE_STRETCH_MS = 1500;
constexpr int WAKE_LOOK_PX = 10;
bool wakeStretchActive = false;
uint32_t wakeStretchStartedAt = 0;
// Occasional double blink.
constexpr long DOUBLE_BLINK_PERCENT = 12;
constexpr uint32_t DOUBLE_BLINK_GAP_MS = 110;
bool doubleBlinkQueued = false;
// Petting: a slow contented sway that stays continuous across purr cycles.
constexpr uint32_t PETTED_SWAY_PERIOD_MS = 2400;
constexpr int PETTED_SWAY_PX = 3;
constexpr float PETTED_BREATH_LID = 0.04f;

float easeToward(float current, float target, uint32_t dtMs, float tauMs) {
  return lerpf(current, target, 1.0f - expf(-float(dtMs) / tauMs));
}

void scheduleNextBlink(uint32_t now) { nextBlinkAt = now + random(1800, 4200); }
void scheduleNextLook(uint32_t now) { nextLookAt = now + random(600, 1600); }
void scheduleNextDrowsy(uint32_t now) { nextDrowsyAt = now + random(7000, 14000); }

uint16_t backgroundColorAt(int y) {
  float t = float(y) / float(SCREEN_H - 1);
  uint8_t r = uint8_t((1.0f - t) * 6 + t * 10);
  uint8_t g = uint8_t((1.0f - t) * 10 + t * 18);
  uint8_t b = uint8_t((1.0f - t) * 22 + t * 38);
  return rgb565(r, g, b);
}
void fillBackgroundRegion(GFXcanvas16 &target, int screenY, int width, int height) {
  for (int y = 0; y < height; y++)
    target.drawFastHLine(0, y, width, backgroundColorAt(screenY + y));
}
void drawStaticBackground() {
  for (int y = 0; y < SCREEN_H; y++)
    tft.drawFastHLine(0, y, SCREEN_W, backgroundColorAt(y));
  for (int i = 0; i < 6; i++) {
    int x = 25 + i * 45;
    int y = 24 + (i % 2) * 8;
    tft.fillCircle(x, y, 2, COL_ACCENT);
  }
  tft.fillRoundRect((SCREEN_W / 2) - 16, SCREEN_H - 22, 32, 8, 4, COL_PANEL);
}
void drawSpark(GFXcanvas16 &target, int x, int y, int len, uint16_t c) {
  target.drawLine(x - len, y, x + len, y, c);
  target.drawLine(x, y - len, x, y + len, c);
  target.drawLine(x - len / 2, y - len / 2, x + len / 2, y + len / 2, c);
  target.drawLine(x - len / 2, y + len / 2, x + len / 2, y - len / 2, c);
}
struct EyeExpressionParams {
  float topLid;
  float bottomLid;
  int irisRadius;
  int pupilRadius;
  int pupilBiasX;
  int pupilBiasY;
  bool reactiveIris;
  bool showSpark;
};
EyeExpressionParams entryFromLeft{}, entryFromRight{};
EyeExpressionParams exitFromLeft{}, exitFromRight{};
EyeExpressionParams exitToLeft{}, exitToRight{};
EyeExpressionParams renderedLeft{}, renderedRight{};
uint16_t currentIrisColor = COL_IRIS;
bool hasRenderedEyes = false;

// Shared easing and geometry interpolation for reaction entry and exit.
[[maybe_unused]] float smoothstep01(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

[[maybe_unused]] EyeExpressionParams interpolateExpressionParams(
    const EyeExpressionParams &from, const EyeExpressionParams &to, float progress) {
  progress = clampf(progress, 0.0f, 1.0f);
  EyeExpressionParams result = progress < 0.5f ? from : to;
  // Copy endpoints exactly; interpolate only intermediate numeric snapshots.
  if (progress > 0.0f && progress < 1.0f) {
    result.topLid = lerpf(from.topLid, to.topLid, progress);
    result.bottomLid = lerpf(from.bottomLid, to.bottomLid, progress);
    result.irisRadius = static_cast<int>(lroundf(lerpf(float(from.irisRadius), float(to.irisRadius), progress)));
    result.pupilRadius = static_cast<int>(lroundf(lerpf(float(from.pupilRadius), float(to.pupilRadius), progress)));
    result.pupilBiasX = static_cast<int>(lroundf(lerpf(float(from.pupilBiasX), float(to.pupilBiasX), progress)));
    result.pupilBiasY = static_cast<int>(lroundf(lerpf(float(from.pupilBiasY), float(to.pupilBiasY), progress)));
  }
  result.topLid = clampf(result.topLid, 0.0f, 1.0f);
  result.bottomLid = clampf(result.bottomLid, 0.0f, 1.0f);
  if (result.irisRadius < 1) result.irisRadius = 1;
  if (result.pupilRadius < 1) result.pupilRadius = 1;
  return result;
}

float reactionProgress01(uint32_t now) {
  return clampf(float(static_cast<uint32_t>(now - reactionStartedAt)) /
                reactionDurationMs, 0.0f, 1.0f);
}

[[maybe_unused]] float reactionPresentationProgress01(uint32_t now) {
  const uint32_t elapsed = static_cast<uint32_t>(now - reactionStartedAt);
  if (elapsed <= REACTION_ENTER_TRANSITION_MS) return 0.0f;
  return clampf(float(elapsed - REACTION_ENTER_TRANSITION_MS) /
                (reactionDurationMs - REACTION_ENTER_TRANSITION_MS), 0.0f, 1.0f);
}

// One shot: zero at either endpoint, smoothly rising to one at the midpoint.
[[maybe_unused]] float smoothPulse(float progress) {
  progress = clampf(progress, 0.0f, 1.0f);
  return smoothstep01(1.0f - fabsf(2.0f * progress - 1.0f));
}

// Mood resting pose: offsets from the neutral idle face, held between reactions
// so Buddy's disposition stays readable while nothing else is happening.
struct MoodRestPose {
  float topLid;
  float bottomLid;
  float irisDelta;
  float pupilDelta;
  float biasX;
  float biasY;
  // Iris tint, as RGB offsets from the base iris colour.
  float irisR;
  float irisG;
  float irisB;
};
constexpr uint32_t MOOD_MOMENT_DELAY_MS = 400;
constexpr uint32_t MOOD_MOMENT_MS = 900;
constexpr uint32_t SLEEPY_MOOD_MOMENT_MS = 1300;
BuddyMood faceMood = BuddyMood::Calm;
BuddyMood shownMood = BuddyMood::Calm;
BuddyMood momentMood = BuddyMood::Calm;
MoodRestPose restPose{};
MoodRestPose momentFrom{};
bool moodMomentActive = false;
uint32_t moodMomentStartedAt = 0;
uint32_t idleSettledAt = 0;
bool snapMoodOnNextUpdate = false;

MoodRestPose moodRestPose(BuddyMood mood) {
  switch (mood) {
    // Engaged warms toward teal, Grumpy cools to slate, Sleepy desaturates.
    case BuddyMood::Engaged: return {0.0f, 0.06f, 1.0f, 0.0f, 0.0f, -3.0f, 30.0f, 25.0f, -20.0f};
    case BuddyMood::Grumpy: return {0.20f, 0.06f, 0.0f, -1.0f, 0.0f, 2.0f, 20.0f, -35.0f, -70.0f};
    case BuddyMood::Sleepy: return {0.30f, 0.08f, -1.0f, 0.0f, 0.0f, 4.0f, 25.0f, -15.0f, -60.0f};
    case BuddyMood::Calm:
    default: return {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  }
}

uint32_t moodMomentDuration(BuddyMood mood) {
  return mood == BuddyMood::Sleepy ? SLEEPY_MOOD_MOMENT_MS : MOOD_MOMENT_MS;
}

float moodMomentProgress01(uint32_t now) {
  return clampf(float(static_cast<uint32_t>(now - moodMomentStartedAt)) /
                moodMomentDuration(momentMood), 0.0f, 1.0f);
}

MoodRestPose currentRestPose(uint32_t now) {
  if (!moodMomentActive) return restPose;
  const MoodRestPose to = moodRestPose(momentMood);
  const float t = smoothstep01(moodMomentProgress01(now));
  return {lerpf(momentFrom.topLid, to.topLid, t), lerpf(momentFrom.bottomLid, to.bottomLid, t),
          lerpf(momentFrom.irisDelta, to.irisDelta, t), lerpf(momentFrom.pupilDelta, to.pupilDelta, t),
          lerpf(momentFrom.biasX, to.biasX, t), lerpf(momentFrom.biasY, to.biasY, t),
          lerpf(momentFrom.irisR, to.irisR, t), lerpf(momentFrom.irisG, to.irisG, t),
          lerpf(momentFrom.irisB, to.irisB, t)};
}

// One-shot gesture layered over the rest-pose glide, named for the new mood:
// Engaged perks up, Grumpy huffs, Sleepy takes a heavy blink, Calm sighs.
MoodRestPose moodMomentAccent(uint32_t now, bool isLeftEye) {
  if (!moodMomentActive) return {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  const float p = smoothPulse(moodMomentProgress01(now));
  switch (momentMood) {
    case BuddyMood::Engaged:
      return {0.0f, 0.0f, 2.0f * p, 1.0f * p, 0.0f, -4.0f * p, 0.0f, 0.0f, 0.0f};
    case BuddyMood::Grumpy:
      return {(isLeftEye ? 0.26f : 0.18f) * p, 0.12f * p, 0.0f, -1.0f * p,
              (isLeftEye ? 2.0f : -2.0f) * p, 2.0f * p, 0.0f, 0.0f, 0.0f};
    case BuddyMood::Sleepy:
      return {0.50f * p, 0.10f * p, 0.0f, 0.0f, 0.0f, 4.0f * p, 0.0f, 0.0f, 0.0f};
    case BuddyMood::Calm:
    default:
      return {0.30f * p, 0.10f * p, 0.0f, 0.0f, 0.0f, 2.0f * p, 0.0f, 0.0f, 0.0f};
  }
}

float wakeStretchProgress01(uint32_t now) {
  return clampf(float(static_cast<uint32_t>(now - wakeStretchStartedAt)) / WAKE_STRETCH_MS,
                0.0f, 1.0f);
}

// 0..1 eye-opening after the wake blink; zero at both ends of the stretch.
float wakeStretchWiden(uint32_t now) {
  if (!wakeStretchActive) return 0.0f;
  return smoothPulse((wakeStretchProgress01(now) - 0.12f) / 0.50f);
}

float wakeStretchLookX(uint32_t now) {
  if (!wakeStretchActive) return 0.0f;
  const float p = wakeStretchProgress01(now);
  return WAKE_LOOK_PX * (smoothPulse((p - 0.55f) / 0.30f) - smoothPulse((p - 0.30f) / 0.30f));
}

uint16_t moodIrisColor(uint32_t now) {
  const MoodRestPose pose = currentRestPose(now);
  return rgb565(uint8_t(clampf(64.0f + pose.irisR, 0.0f, 255.0f)),
                uint8_t(clampf(170.0f + pose.irisG, 0.0f, 255.0f)),
                uint8_t(clampf(255.0f + pose.irisB, 0.0f, 255.0f)));
}

// Neutral idle geometry tinted by mood; blinks and drowsiness close from the tint.
EyeExpressionParams restingFaceParams(uint32_t now, bool isLeftEye) {
  const MoodRestPose pose = currentRestPose(now);
  const MoodRestPose accent = moodMomentAccent(now, isLeftEye);
  const float widen = wakeStretchWiden(now);
  const float open = 1.0f - widen;
  const float top = clampf((pose.topLid + accent.topLid) * open, 0.0f, 1.0f);
  const float bottom = clampf((pose.bottomLid + accent.bottomLid) * open, 0.0f, 1.0f);
  return {top + lidAmount * (1.0f - top), bottom + lidAmount * (1.0f - bottom),
          18 + int(lroundf(pose.irisDelta + accent.irisDelta + 2.0f * widen)),
          8 + int(lroundf(pose.pupilDelta + accent.pupilDelta)),
          int(lroundf(pose.biasX + accent.biasX + wakeStretchLookX(now))),
          int(lroundf(pose.biasY + accent.biasY - 3.0f * widen)),
          false, false};
}

void settleMoodMoment(uint32_t now) {
  if (!moodMomentActive) return;
  // Freeze the glide where it is; the moment replays once Buddy is idle again.
  restPose = currentRestPose(now);
  moodMomentActive = false;
}

EyeExpressionParams applyReactionMicroAnimation(
    FaceExpression expression, bool isLeftEye, uint32_t now,
    EyeExpressionParams params) {
  if (expression == FaceExpression::Normal) return params;
  const float progress = reactionPresentationProgress01(now);
  const float k = reactionIntensity;
  switch (expression) {
    case FaceExpression::Happy: {
      const float bounce = smoothPulse(progress) * k;
      params.pupilBiasY -= int(lroundf(HAPPY_BOUNCE_PX * bounce));
      params.bottomLid += HAPPY_LID_LIFT * bounce;
      break;
    }
    case FaceExpression::Curious: {
      const float shift = smoothstep01((progress - 0.35f) / 0.25f) * k;
      params.pupilBiasX += (curiousShiftRight ? 1 : -1) * int(lroundf(CURIOUS_SHIFT_PX * shift));
      break;
    }
    case FaceExpression::Annoyed: {
      const float tighten = smoothstep01(progress / 0.5f) * k;
      params.topLid += ANNOYED_TOP_TIGHTEN * tighten;
      params.bottomLid += ANNOYED_BOTTOM_TIGHTEN * tighten;
      break;
    }
    case FaceExpression::Confused: {
      const float diverge = smoothstep01((progress - 0.30f) / 0.35f) * k;
      params.pupilBiasX += (isLeftEye ? -1 : 1) * int(lroundf(CONFUSED_DIVERGE_PX * diverge));
      break;
    }
    case FaceExpression::Startled: {
      const float settle = smoothstep01(progress) * k;
      params.topLid += STARTLED_TOP_SETTLE * settle;
      params.bottomLid += STARTLED_BOTTOM_SETTLE * settle;
      params.pupilRadius += int(lroundf(STARTLED_PUPIL_GROWTH_PX * settle));
      params.irisRadius += int(lroundf(STARTLED_IRIS_GROWTH_PX * settle));
      params.showSpark = progress < STARTLED_SPARK_END_PROGRESS;
      break;
    }
    case FaceExpression::Petted: {
      // Wall-clock sway so back-to-back purr cycles join seamlessly.
      const float phase = float(now % PETTED_SWAY_PERIOD_MS) / PETTED_SWAY_PERIOD_MS;
      params.pupilBiasX += int(lroundf(PETTED_SWAY_PX * sinf(phase * 6.2831853f)));
      params.topLid += PETTED_BREATH_LID * (0.5f + 0.5f * cosf(phase * 6.2831853f * 2.0f));
      break;
    }
    case FaceExpression::ContentBlink: {
      const float blink = smoothPulse((progress - 0.10f) / 0.45f);
      params.topLid += (1.0f - params.topLid) * blink;
      params.bottomLid += 0.15f * blink;
      break;
    }
    default:
      return params;
  }
  params.topLid = clampf(params.topLid, 0.0f, 1.0f);
  params.bottomLid = clampf(params.bottomLid, 0.0f, 1.0f);
  return params;
}

EyeExpressionParams baseExpressionParams(FaceExpression expression, bool isLeftEye,
                                    uint32_t now) {
  switch (expression) {
    case FaceExpression::Happy:
      return {0.10f, 0.34f, 18, 8, isLeftEye ? 3 : -3, -6, false, false};
    case FaceExpression::Curious:
      return isLeftEye
          ? EyeExpressionParams{0.02f, 0.06f, 19, 9, 5, -8, false, false}
          : EyeExpressionParams{0.28f, 0.12f, 19, 9, 5, -8, false, false};
    case FaceExpression::Daydreaming:
      return isLeftEye
          ? EyeExpressionParams{0.38f, 0.10f, 18, 8, 10, -13, false, false}
          : EyeExpressionParams{0.44f, 0.14f, 18, 8, 10, -13, false, false};
    case FaceExpression::Annoyed:
      return isLeftEye
          ? EyeExpressionParams{0.46f, 0.03f, 18, 8, -6, 2, false, false}
          : EyeExpressionParams{0.60f, 0.0f, 18, 8, 3, 2, false, false};
    case FaceExpression::Startled:
      return {0.0f, 0.0f, 17, 4, 0, 0, true, true};
    case FaceExpression::Suspicious:
      return isLeftEye
          ? EyeExpressionParams{0.42f, 0.08f, 18, 7, 12, 1, false, false}
          : EyeExpressionParams{0.08f, 0.03f, 18, 7, 12, 1, false, false};
    case FaceExpression::Confused:
      return isLeftEye
          ? EyeExpressionParams{0.12f, 0.04f, 19, 9, -7, -9, false, false}
          : EyeExpressionParams{0.52f, 0.10f, 19, 9, 8, 5, false, false};
    case FaceExpression::SideGlance:
      return {0.08f, 0.06f, 18, 8, sideGlanceRight ? 16 : -16, 0, false, false};
    case FaceExpression::Bored:
      return {isLeftEye ? 0.66f : 0.70f, 0.12f, 17, 8, -3, 9, false, false};
    case FaceExpression::SuspiciousGlance:
      return {isLeftEye ? 0.50f : 0.56f, 0.18f, 17, 6, -16, 0, false, false};
    case FaceExpression::AnnoyedSquint:
      return {isLeftEye ? 0.38f : 0.44f, 0.26f, 18, 7, -2, 2, false, false};
    case FaceExpression::SleepyDrift: {
      const float progress = reactionProgress01(now);
      const float drift = progress < 0.65f ? progress / 0.65f
          : 1.0f - 0.65f * ((progress - 0.65f) / 0.35f);
      return {0.48f + drift * 0.22f, 0.12f, 17, 8,
              int(drift * 5), int(drift * 13), false, false};
    }
    case FaceExpression::ExcitedScanning: {
      const float progress = reactionProgress01(now);
      // Smooth left -> center -> right -> center over the existing duration.
      const float scan = progress < 0.66f ? lerpf(-15.0f, 15.0f, progress / 0.66f)
          : lerpf(15.0f, 0.0f, (progress - 0.66f) / 0.34f);
      return {0.02f, 0.02f, 19, 8, int(scan), -3, false, false};
    }
    case FaceExpression::Petted:
      return {0.30f, 0.40f, 18, 7, isLeftEye ? 3 : -3, -4, false, false};
    case FaceExpression::ContentBlink:
      return {0.10f, 0.34f, 18, 8, isLeftEye ? 3 : -3, -6, false, false};
    case FaceExpression::Normal:
    default:
      return restingFaceParams(now, isLeftEye);
  }
}

EyeExpressionParams expressionParams(FaceExpression expression, bool isLeftEye,
                                    uint32_t now) {
  const EyeExpressionParams base = baseExpressionParams(expression, isLeftEye, now);
  if (expression == FaceExpression::Normal) return base;
  return applyReactionMicroAnimation(expression, isLeftEye, now, base);
}

EyeExpressionParams visualExpressionParams(uint32_t now, BuddyReaction reaction,
                                          bool isLeftEye) {
  if (reactionExitTransitionActive) {
    const float progress = smoothstep01(float(static_cast<uint32_t>(now - reactionExitStartedAt)) /
                                       REACTION_EXIT_TRANSITION_MS);
    return interpolateExpressionParams(isLeftEye ? exitFromLeft : exitFromRight,
                                       isLeftEye ? exitToLeft : exitToRight, progress);
  }
  EyeExpressionParams target = expressionParams(
      reaction == BuddyReaction::Generic ? activeExpression : FaceExpression::Normal,
      isLeftEye, now);
  if (reaction == BuddyReaction::Generic && reactionEntryTransitionActive) {
    const float progress = smoothstep01(float(static_cast<uint32_t>(now - reactionEntryStartedAt)) /
                                       REACTION_ENTER_TRANSITION_MS);
    return interpolateExpressionParams(isLeftEye ? entryFromLeft : entryFromRight, target, progress);
  }
  if (reaction != BuddyReaction::Generic) {
    target.pupilBiasX = int(clampf(pupilX + target.pupilBiasX, -17, 17));
    target.pupilBiasY = int(clampf(pupilY + target.pupilBiasY, -24, 24));
  }
  return target;
}

void drawEye(GFXcanvas16 &target, int cx, int cy, int w, int h,
             const EyeExpressionParams &params) {
  int x = cx - (w / 2);
  int y = cy - (h / 2);
  if (params.topLid > 0.93f && params.bottomLid > 0.93f) {
    target.fillRoundRect(x + 10, cy - 2, w - 20, 5, 2, COL_EYE_LINE);
    target.drawFastHLine(cx - 18, cy + 5, 36, COL_HILITE);
    return;
  }
  target.fillRoundRect(x, y, w, h, eyeCorner, COL_EYE_WHITE);
  target.drawRoundRect(x, y, w, h, eyeCorner, COL_EYE_LINE);
  int pupilRangeX = (w / 2) - 22;
  int pupilRangeY = (h / 2) - 24;
  int px = cx + int(clampf(params.pupilBiasX, -pupilRangeX, pupilRangeX));
  int py = cy + int(clampf(params.pupilBiasY, -pupilRangeY, pupilRangeY));
  uint16_t irisColor = params.reactiveIris ? COL_IRIS_REACT : currentIrisColor;
  target.fillCircle(px, py, params.irisRadius, irisColor);
  target.fillCircle(px, py, params.pupilRadius, COL_PUPIL);
  target.fillCircle(px - 5, py - 5, 4, COL_HILITE);
  target.fillCircle(px + 6, py + 6, 2, COL_HILITE);
  target.drawFastHLine(x + 16, y + h - 12, w - 32, COL_HILITE);
  int topCover = int((h / 2.0f) * params.topLid);
  int bottomCover = int((h / 2.0f) * params.bottomLid);
  if (topCover > 0) {
    target.fillRoundRect(x - 1, y - 1, w + 2, topCover + 4, eyeCorner, COL_BG_TOP);
    target.drawFastHLine(x + 10, y + topCover, w - 20, COL_EYE_LINE);
  }
  if (bottomCover > 0) {
    target.fillRoundRect(x - 1, y + h - bottomCover - 3, w + 2,
                         bottomCover + 5, eyeCorner, COL_BG_BOTTOM);
    target.drawFastHLine(x + 10, y + h - bottomCover, w - 20, COL_EYE_LINE);
  }
}
void renderEyeRegion(int screenCenterX, int screenCenterY, int sparkCenterX,
                     const EyeExpressionParams &params) {
  int screenX = screenCenterX - (EYE_REGION_W / 2);
  fillBackgroundRegion(eyeCanvas, EYE_REGION_Y, EYE_REGION_W, EYE_REGION_H);
  drawEye(eyeCanvas, EYE_REGION_W / 2, screenCenterY - EYE_REGION_Y,
          eyeW, eyeH, params);
  if (params.showSpark)
    drawSpark(eyeCanvas, sparkCenterX - screenX,
              screenCenterY - 56 - EYE_REGION_Y, 5, COL_IRIS_REACT);
  tft.drawRGBBitmap(screenX, EYE_REGION_Y, eyeCanvas.getBuffer(), EYE_REGION_W, EYE_REGION_H);
}
void renderSleepZ(uint32_t now, BuddyCoreState coreState) {
  constexpr int Z_X = 212;
  constexpr int Z_Y = 31;
  fillBackgroundRegion(effectCanvas, Z_Y, EFFECT_REGION_W, EFFECT_REGION_H);
  if (coreState == BuddyCoreState::Sleeping) {
    effectCanvas.setTextColor(COL_SLEEP);
    effectCanvas.setTextSize(2);
    effectCanvas.setCursor(0, 34 + int(sinf(now * 0.003f) * 3.0f) - Z_Y);
    effectCanvas.print("Z");
  }
  tft.drawRGBBitmap(Z_X, Z_Y, effectCanvas.getBuffer(), EFFECT_REGION_W, EFFECT_REGION_H);
}
float moodReactionTempo(BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Engaged: return 0.90f;
    case BuddyMood::Sleepy: return 1.15f;
    case BuddyMood::Grumpy:
    case BuddyMood::Calm:
    default: return 1.0f;
  }
}

float moodReactionIntensity(BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Engaged: return 1.25f;
    case BuddyMood::Sleepy: return 0.70f;
    case BuddyMood::Grumpy:
    case BuddyMood::Calm:
    default: return 1.0f;
  }
}

// Lids droop, rise once as Buddy fights it, then close fully.
float fallAsleepLid(float progress) {
  const float droop = lerpf(fallAsleepFromLid, 0.65f, smoothstep01(progress / 0.45f));
  const float resist = 0.20f * smoothPulse((progress - 0.40f) / 0.25f);
  return clampf(lerpf(droop - resist, 1.0f, smoothstep01((progress - 0.60f) / 0.40f)),
                0.0f, 1.0f);
}

int sleepNodOffsetPx(uint32_t now) {
  if (!fallingAsleep) return 0;
  const float progress = clampf(float(static_cast<uint32_t>(now - fallAsleepStartedAt)) /
                                FALL_ASLEEP_MS, 0.0f, 1.0f);
  return int(lroundf(SLEEP_NOD_PX * smoothPulse((progress - 0.35f) / 0.60f)));
}

void renderFrame(uint32_t now, BuddyCoreState coreState, BuddyReaction reaction) {
  int bobY = coreState == BuddyCoreState::Sleeping
      ? int(sinf(now * 0.0022f) * 3.0f) : int(sinf(now * 0.0045f) * 1.5f);
  int y = eyeY + bobY + sleepNodOffsetPx(now);
  currentIrisColor = moodIrisColor(now);
  renderedLeft = visualExpressionParams(now, reaction, true);
  renderedRight = visualExpressionParams(now, reaction, false);
  hasRenderedEyes = true;
  renderEyeRegion(leftEyeX, y, leftEyeX - 34, renderedLeft);
  renderEyeRegion(rightEyeX, y, rightEyeX + 34, renderedRight);
  renderSleepZ(now, coreState);
}
void updateBacklight(uint32_t dtMs) {
  backlightLevel = easeToward(backlightLevel, float(backlightTarget), dtMs, BACKLIGHT_TAU_MS);
  if (fabsf(backlightLevel - float(backlightTarget)) < 0.5f) backlightLevel = float(backlightTarget);
  const int level = constrain(int(lroundf(backlightLevel)), 0, 255);
  if (level == backlightCurrent) return;
  backlightCurrent = level;
  analogWrite(TFT_BL, backlightCurrent);
}
}

void beginFaceRenderer() {
  pinMode(TFT_BL, OUTPUT);
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(TFT_BL, 255);
  SPI.setSCK(TFT_SCLK);
  SPI.setTX(TFT_MOSI);
  SPI.begin();
  tft.init(240, 280);
  tft.setRotation(1);
  drawStaticBackground();
}
void scheduleFaceBehavior(uint32_t now) {
  doubleBlinkQueued = false;
  scheduleNextBlink(now); scheduleNextLook(now); scheduleNextDrowsy(now);
}
void startFaceReaction(uint32_t now, FaceExpression expression) {
  entryFromLeft = hasRenderedEyes ? renderedLeft : visualExpressionParams(now, BuddyReaction::Idle, true);
  entryFromRight = hasRenderedEyes ? renderedRight : visualExpressionParams(now, BuddyReaction::Idle, false);
  reactionExitTransitionActive = false;
  reactionEntryStartedAt = now;
  reactionEntryTransitionActive = true;
  reactionStartedAt = now;
  reactionDurationMs = static_cast<uint32_t>(lroundf(REACTION_DURATION_MS * moodReactionTempo(faceMood))) +
                       static_cast<uint32_t>(random(0, REACTION_JITTER_MS + 1));
  reactionIntensity = moodReactionIntensity(faceMood) *
      (1.0f + float(random(0, REACTION_INTENSITY_JITTER_PERCENT + 1)) / 100.0f);
  reactUntil = now + reactionDurationMs;
  if (expression == FaceExpression::SideGlance) sideGlanceRight = !sideGlanceRight;
  if (expression == FaceExpression::Curious) curiousShiftRight = !curiousShiftRight;
  activeExpression = expression;
  // Apparent ambient pupil position is frozen in the source snapshots.
  pupilX = 0;
  pupilY = 0;
  pupilTargetX = 0;
  pupilTargetY = 0;
  drowsyUntil = 0;
  settleMoodMoment(now);
  wakeStretchActive = false;
}
bool isFaceReactionFinished(uint32_t now) { return timeReached(now, reactUntil); }
void finishFaceReaction(uint32_t now) {
  exitFromLeft = hasRenderedEyes ? renderedLeft : visualExpressionParams(now, BuddyReaction::Generic, true);
  exitFromRight = hasRenderedEyes ? renderedRight : visualExpressionParams(now, BuddyReaction::Generic, false);
  reactionEntryTransitionActive = false;
  activeExpression = FaceExpression::Normal;
  settleMoodMoment(now);
  wakeStretchActive = false;
  // Freeze a relaxed, centered idle destination and discard stale micro-actions.
  blinkActive = false;
  drowsyUntil = now;
  lidAmount = lidEased = 0;
  pupilX = pupilY = pupilTargetX = pupilTargetY = 0;
  exitToLeft = expressionParams(FaceExpression::Normal, true, now);
  exitToRight = expressionParams(FaceExpression::Normal, false, now);
  reactionExitStartedAt = now;
  reactionExitTransitionActive = true;
  // Fresh idle deadlines are scheduled only after the visual exit completes.
}
void enterSleepFace(uint32_t now) {
  reactionExitTransitionActive = false;
  reactionEntryTransitionActive = false;
  backlightTarget = SLEEP_BACKLIGHT;
  blinkActive = false;
  reactUntil = 0;
  activeExpression = FaceExpression::Normal;
  moodMomentActive = false;
  wakeStretchActive = false;
  fallingAsleep = true;
  fallAsleepStartedAt = now;
  fallAsleepFromLid = lidAmount;
}
void wakeFace(uint32_t now) {
  reactionExitTransitionActive = false;
  reactionEntryTransitionActive = false;
  backlightTarget = 255;
  blinkActive = true;
  blinkStart = now;
  blinkDuration = 260;
  activeExpression = FaceExpression::Normal;
  fallingAsleep = false;
  wakeStretchActive = true;
  wakeStretchStartedAt = now;
  // The wake blink and stretch are the moment; open straight into the current mood.
  snapMoodOnNextUpdate = true;
  idleSettledAt = now;
  scheduleFaceBehavior(now);
}
void setFaceMood(BuddyMood mood) { faceMood = mood; }
void updateFaceRenderer(uint32_t now, BuddyCoreState coreState, BuddyReaction reaction) {
  const uint32_t sinceUpdate = static_cast<uint32_t>(now - lastUpdateAt);
  const uint32_t dt = !hasUpdated || sinceUpdate > MAX_EASE_STEP_MS ? MAX_EASE_STEP_MS : sinceUpdate;
  hasUpdated = true;
  lastUpdateAt = now;

  if (reactionExitTransitionActive &&
      static_cast<uint32_t>(now - reactionExitStartedAt) >= REACTION_EXIT_TRANSITION_MS) {
    reactionExitTransitionActive = false;
    // exitTo already carries the mood rest pose; ambient motion restarts at zero.
    lidAmount = lidEased = 0;
    pupilX = pupilTargetX = 0;
    pupilY = pupilTargetY = 0;
    idleSettledAt = now;
    scheduleFaceBehavior(now);
  }
  if (reactionEntryTransitionActive &&
      static_cast<uint32_t>(now - reactionEntryStartedAt) >= REACTION_ENTER_TRANSITION_MS) {
    reactionEntryTransitionActive = false;
  }
  if (coreState == BuddyCoreState::Sleeping || snapMoodOnNextUpdate) {
    // Closed or just-waking eyes take the mood pose without a moment.
    snapMoodOnNextUpdate = false;
    moodMomentActive = false;
    shownMood = faceMood;
    restPose = moodRestPose(faceMood);
  }
  if (wakeStretchActive &&
      (coreState != BuddyCoreState::Awake ||
       static_cast<uint32_t>(now - wakeStretchStartedAt) >= WAKE_STRETCH_MS)) {
    wakeStretchActive = false;
    idleSettledAt = now;
    scheduleFaceBehavior(now);
  }
  if (moodMomentActive &&
      static_cast<uint32_t>(now - moodMomentStartedAt) >= moodMomentDuration(momentMood)) {
    moodMomentActive = false;
    restPose = moodRestPose(momentMood);
    shownMood = momentMood;
    idleSettledAt = now;
    scheduleFaceBehavior(now);
  }
  // Mood shifts during reactions wait until Buddy has settled back to idle.
  if (coreState == BuddyCoreState::Awake && reaction == BuddyReaction::Idle &&
      !reactionExitTransitionActive && !moodMomentActive && !wakeStretchActive &&
      faceMood != shownMood &&
      static_cast<uint32_t>(now - idleSettledAt) >= MOOD_MOMENT_DELAY_MS) {
    momentFrom = restPose;
    momentMood = faceMood;
    moodMomentStartedAt = now;
    moodMomentActive = true;
    // Center and open the eyes so the gesture reads cleanly.
    drowsyUntil = now;
    pupilTargetX = pupilTargetY = 0;
  }
  // Scripted idle moments own the eyes; ambient micro-actions wait for them.
  const bool idleFree = coreState == BuddyCoreState::Awake && reaction == BuddyReaction::Idle &&
      !reactionExitTransitionActive && !moodMomentActive && !wakeStretchActive;
  if (idleFree && !blinkActive && timeReached(now, nextBlinkAt)) {
    blinkActive = true;
    blinkStart = now;
    blinkDuration = random(140, 220);
    if (!doubleBlinkQueued && random(100) >= 100 - DOUBLE_BLINK_PERCENT) {
      doubleBlinkQueued = true;
      nextBlinkAt = now + blinkDuration + DOUBLE_BLINK_GAP_MS;
    } else {
      doubleBlinkQueued = false;
      scheduleNextBlink(now + blinkDuration);
    }
  }
  if (idleFree && timeReached(now, nextDrowsyAt)) {
    drowsyUntil = now + random(1300, 2800);
    scheduleNextDrowsy(now);
  }
  float blinkAmt = 0.0f;
  if (blinkActive) {
    float p = float(now - blinkStart) / float(blinkDuration);
    if (p >= 1.0f) blinkActive = false;
    else blinkAmt = sinf(p * 3.1415926f);
  }
  float sleepyAmt = 0.0f;
  if (idleFree && !timeReached(now, drowsyUntil))
    sleepyAmt = 0.25f + 0.08f * (0.5f + 0.5f * sinf(now * 0.004f));
  if (coreState == BuddyCoreState::Sleeping && fallingAsleep) {
    const uint32_t elapsed = static_cast<uint32_t>(now - fallAsleepStartedAt);
    if (elapsed >= FALL_ASLEEP_MS) {
      fallingAsleep = false;
      lidEased = 1.0f;
      sleepBreathStartedAt = now;
    } else {
      lidEased = fallAsleepLid(float(elapsed) / FALL_ASLEEP_MS);
    }
  } else {
    float targetLid = 0.0f;
    if (coreState == BuddyCoreState::Sleeping) targetLid = 1.0f;
    else if (reaction != BuddyReaction::Generic) targetLid = sleepyAmt;
    lidEased = easeToward(lidEased, targetLid, dt, LID_EASE_TAU_MS);
  }
  lidAmount = max(blinkAmt, lidEased);
  if (coreState == BuddyCoreState::Sleeping && !fallingAsleep) {
    // Slow breathing glow while asleep.
    const float phase = float(static_cast<uint32_t>(now - sleepBreathStartedAt) %
                              SLEEP_BREATH_PERIOD_MS) / SLEEP_BREATH_PERIOD_MS;
    backlightTarget = SLEEP_BACKLIGHT +
        int(lroundf(SLEEP_BREATH_DEPTH * (0.5f - 0.5f * cosf(phase * 6.2831853f))));
  }

  if (reaction == BuddyReaction::Generic) {
    pupilTargetX = 0; pupilTargetY = 0;
  } else if (coreState == BuddyCoreState::Sleeping) {
    pupilTargetX = 0; pupilTargetY = 10;
  } else if (idleFree && timeReached(now, nextLookAt)) {
    pupilTargetX = random(-16, 17);
    pupilTargetY = random(-10, 13);
    if (!timeReached(now, drowsyUntil)) pupilTargetY = random(6, 14);
    scheduleNextLook(now);
  }
  const float pupilTau = reaction == BuddyReaction::Generic ? PUPIL_REACTION_TAU_MS
      : coreState == BuddyCoreState::Sleeping ? PUPIL_SLEEP_TAU_MS : PUPIL_IDLE_TAU_MS;
  pupilX = easeToward(pupilX, pupilTargetX, dt, pupilTau);
  pupilY = easeToward(pupilY, pupilTargetY, dt, pupilTau);
  updateBacklight(dt);
  if (now - lastFrameAt >= 50) {
    lastFrameAt = now;
    renderFrame(now, coreState, reaction);
  }
}

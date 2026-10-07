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
FaceExpression activeExpression = FaceExpression::Normal;
uint32_t lastFrameAt = 0;
int backlightCurrent = 255;
int backlightTarget = 255;

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
                REACTION_DURATION_MS, 0.0f, 1.0f);
}

[[maybe_unused]] float reactionPresentationProgress01(uint32_t now) {
  const uint32_t elapsed = static_cast<uint32_t>(now - reactionStartedAt);
  if (elapsed <= REACTION_ENTER_TRANSITION_MS) return 0.0f;
  return clampf(float(elapsed - REACTION_ENTER_TRANSITION_MS) /
                (REACTION_DURATION_MS - REACTION_ENTER_TRANSITION_MS), 0.0f, 1.0f);
}

// One shot: zero at either endpoint, smoothly rising to one at the midpoint.
[[maybe_unused]] float smoothPulse(float progress) {
  progress = clampf(progress, 0.0f, 1.0f);
  return smoothstep01(1.0f - fabsf(2.0f * progress - 1.0f));
}

EyeExpressionParams applyReactionMicroAnimation(
    FaceExpression expression, bool /*isLeftEye*/, uint32_t /*now*/,
    EyeExpressionParams params) {
  if (expression == FaceExpression::Normal) return params;
  // Future expression adjustments use presentation progress and procedural curves
  // here. This infrastructure pass deliberately preserves every existing target.
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
    case FaceExpression::Normal:
    default:
      return {lidAmount, lidAmount, 18, 8, 0, 0, false, false};
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
  uint16_t irisColor = params.reactiveIris ? COL_IRIS_REACT : COL_IRIS;
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
void renderFrame(uint32_t now, BuddyCoreState coreState, BuddyReaction reaction) {
  int bobY = coreState == BuddyCoreState::Sleeping
      ? int(sinf(now * 0.0022f) * 3.0f) : int(sinf(now * 0.0045f) * 1.5f);
  int y = eyeY + bobY;
  renderedLeft = visualExpressionParams(now, reaction, true);
  renderedRight = visualExpressionParams(now, reaction, false);
  hasRenderedEyes = true;
  renderEyeRegion(leftEyeX, y, leftEyeX - 34, renderedLeft);
  renderEyeRegion(rightEyeX, y, rightEyeX + 34, renderedRight);
  renderSleepZ(now, coreState);
}
void updateBacklight() {
  if (backlightCurrent == backlightTarget) return;
  int diff = backlightTarget - backlightCurrent;
  if (abs(diff) < 2) backlightCurrent = backlightTarget;
  else backlightCurrent += diff / 4;
  backlightCurrent = constrain(backlightCurrent, 0, 255);
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
  scheduleNextBlink(now); scheduleNextLook(now); scheduleNextDrowsy(now);
}
void startFaceReaction(uint32_t now, FaceExpression expression) {
  entryFromLeft = hasRenderedEyes ? renderedLeft : visualExpressionParams(now, BuddyReaction::Idle, true);
  entryFromRight = hasRenderedEyes ? renderedRight : visualExpressionParams(now, BuddyReaction::Idle, false);
  reactionExitTransitionActive = false;
  reactionEntryStartedAt = now;
  reactionEntryTransitionActive = true;
  reactionStartedAt = now;
  reactUntil = now + REACTION_DURATION_MS;
  if (expression == FaceExpression::SideGlance) sideGlanceRight = !sideGlanceRight;
  activeExpression = expression;
  // Apparent ambient pupil position is frozen in the source snapshots.
  pupilX = 0;
  pupilY = 0;
  pupilTargetX = 0;
  pupilTargetY = 0;
  drowsyUntil = 0;
}
bool isFaceReactionFinished(uint32_t now) { return timeReached(now, reactUntil); }
void finishFaceReaction(uint32_t now) {
  exitFromLeft = hasRenderedEyes ? renderedLeft : visualExpressionParams(now, BuddyReaction::Generic, true);
  exitFromRight = hasRenderedEyes ? renderedRight : visualExpressionParams(now, BuddyReaction::Generic, false);
  reactionEntryTransitionActive = false;
  activeExpression = FaceExpression::Normal;
  // Freeze a relaxed, centered idle destination and discard stale micro-actions.
  blinkActive = false;
  drowsyUntil = now;
  lidAmount = 0;
  pupilX = pupilY = pupilTargetX = pupilTargetY = 0;
  exitToLeft = expressionParams(FaceExpression::Normal, true, now);
  exitToRight = expressionParams(FaceExpression::Normal, false, now);
  reactionExitStartedAt = now;
  reactionExitTransitionActive = true;
  // Fresh idle deadlines are scheduled only after the visual exit completes.
}
void enterSleepFace(uint32_t) {
  reactionExitTransitionActive = false;
  reactionEntryTransitionActive = false;
  backlightTarget = 40;
  blinkActive = false;
  reactUntil = 0;
  activeExpression = FaceExpression::Normal;
}
void wakeFace(uint32_t now) {
  reactionExitTransitionActive = false;
  reactionEntryTransitionActive = false;
  backlightTarget = 255;
  blinkActive = true;
  blinkStart = now;
  blinkDuration = 260;
  activeExpression = FaceExpression::Normal;
  scheduleFaceBehavior(now);
}
void updateFaceRenderer(uint32_t now, BuddyCoreState coreState, BuddyReaction reaction) {
  if (reactionExitTransitionActive &&
      static_cast<uint32_t>(now - reactionExitStartedAt) >= REACTION_EXIT_TRANSITION_MS) {
    reactionExitTransitionActive = false;
    lidAmount = exitToLeft.topLid;
    pupilX = pupilTargetX = exitToLeft.pupilBiasX;
    pupilY = pupilTargetY = exitToLeft.pupilBiasY;
    scheduleFaceBehavior(now);
  }
  if (reactionEntryTransitionActive &&
      static_cast<uint32_t>(now - reactionEntryStartedAt) >= REACTION_ENTER_TRANSITION_MS) {
    reactionEntryTransitionActive = false;
  }
  if (coreState == BuddyCoreState::Awake && reaction == BuddyReaction::Idle &&
      !reactionExitTransitionActive &&
      !blinkActive && timeReached(now, nextBlinkAt)) {
    blinkActive = true;
    blinkStart = now;
    blinkDuration = random(140, 220);
    scheduleNextBlink(now + blinkDuration);
  }
  if (coreState == BuddyCoreState::Awake && reaction == BuddyReaction::Idle &&
      !reactionExitTransitionActive &&
      timeReached(now, nextDrowsyAt)) {
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
  if (coreState == BuddyCoreState::Awake && reaction == BuddyReaction::Idle &&
      !reactionExitTransitionActive &&
      !timeReached(now, drowsyUntil))
    sleepyAmt = 0.25f + 0.08f * (0.5f + 0.5f * sinf(now * 0.004f));
  float targetLid = 0.0f;
  if (coreState == BuddyCoreState::Sleeping) targetLid = 1.0f;
  else if (reaction == BuddyReaction::Generic) targetLid = 0.0f;
  else targetLid = max(blinkAmt, sleepyAmt);
  lidAmount = lerpf(lidAmount, targetLid, 0.22f);

  if (reaction == BuddyReaction::Generic) {
    pupilTargetX = 0; pupilTargetY = 0;
  } else if (coreState == BuddyCoreState::Sleeping) {
    pupilTargetX = 0; pupilTargetY = 10;
  } else if (!reactionExitTransitionActive && timeReached(now, nextLookAt)) {
    pupilTargetX = random(-16, 17);
    pupilTargetY = random(-10, 13);
    if (!timeReached(now, drowsyUntil)) pupilTargetY = random(6, 14);
    scheduleNextLook(now);
  }
  float follow = reaction == BuddyReaction::Generic ? 0.22f : 0.08f;
  pupilX = lerpf(pupilX, pupilTargetX, follow);
  pupilY = lerpf(pupilY, pupilTargetY, follow);
  updateBacklight();
  if (now - lastFrameAt >= 50) {
    lastFrameAt = now;
    renderFrame(now, coreState, reaction);
  }
}

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// =========================
// Pin map
// =========================
#define TFT_CS    17
#define TFT_DC    20
#define TFT_RST   21
#define TFT_BL    22
#define TFT_SCLK  18
#define TFT_MOSI  19

#define TOUCH_PIN   5    // TTP223 OUT
#define BUTTON_PIN  7    // push button to GND, use INPUT_PULLUP
#define BUZZER_PIN  15   // passive buzzer signal
#define MIC_PIN     26   // MAX4466 OUT (ADC0)

// =========================
// Display
// =========================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// 1.69" screen in rotation 1 = 280 x 240
constexpr int SCREEN_W = 280;
constexpr int SCREEN_H = 240;

// A single reusable eye-region canvas keeps animated updates flicker-free without
// reserving a full-screen framebuffer. It is large enough for either eye at the
// maximum idle/sleep bob, lid overdraw, and its reaction spark.
constexpr int EYE_REGION_W = 80;
constexpr int EYE_REGION_H = 117;
constexpr int EYE_REGION_Y = 56;
GFXcanvas16 eyeCanvas(EYE_REGION_W, EYE_REGION_H);  // 18,720 bytes

// A second small canvas restores and redraws the animated 12x22 sleep-Z region.
constexpr int EFFECT_REGION_W = 12;
constexpr int EFFECT_REGION_H = 22;
GFXcanvas16 effectCanvas(EFFECT_REGION_W, EFFECT_REGION_H);  // 528 bytes

// =========================
// Color helpers
// =========================
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t COL_BG_TOP     = rgb565(6, 10, 22);
const uint16_t COL_BG_BOTTOM  = rgb565(10, 18, 38);
const uint16_t COL_PANEL      = rgb565(18, 28, 56);
const uint16_t COL_EYE_WHITE  = rgb565(232, 240, 255);
const uint16_t COL_EYE_LINE   = rgb565(95, 180, 255);
const uint16_t COL_IRIS       = rgb565(64, 170, 255);
const uint16_t COL_IRIS_REACT = rgb565(70, 220, 255);
const uint16_t COL_PUPIL      = rgb565(8, 12, 20);
const uint16_t COL_HILITE     = rgb565(255, 255, 255);
const uint16_t COL_SLEEP      = rgb565(140, 210, 255);
const uint16_t COL_ACCENT     = rgb565(40, 90, 180);

// =========================
// Helpers
// =========================
float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

// =========================
// Buddy state
// =========================
enum BuddyMode {
  MODE_IDLE,
  MODE_REACT,
  MODE_SLEEP
};

BuddyMode mode = MODE_IDLE;

// Eye layout
int leftEyeX  = 86;
int rightEyeX = 194;
int eyeY      = 120;
int eyeW      = 78;
int eyeH      = 96;
int eyeCorner = 28;

// Animation state
float pupilX = 0.0f;
float pupilY = 0.0f;
float pupilTargetX = 0.0f;
float pupilTargetY = 0.0f;

float lidAmount = 0.0f; // 0 = open, 1 = closed
bool blinkActive = false;
uint32_t blinkStart = 0;
uint16_t blinkDuration = 180;
uint32_t nextBlinkAt = 0;

uint32_t nextLookAt = 0;
uint32_t drowsyUntil = 0;
uint32_t nextDrowsyAt = 0;

uint32_t reactUntil = 0;
uint32_t nextReactNoteAt = 0;
uint8_t reactStep = 0;

uint32_t lastFrameAt = 0;

// =========================
// Microphone diagnostics
// =========================
struct MicrophoneReading {
  uint16_t average;
  uint16_t minimum;
  uint16_t maximum;
  uint16_t peakToPeak;
  uint32_t timestamp;
};

constexpr uint32_t MIC_SAMPLE_INTERVAL_US = 1000;  // approximately 1 kHz
constexpr uint32_t MIC_REPORT_INTERVAL_MS = 500;   // two reports per second
constexpr uint32_t MIC_DETECTION_WINDOW_MS = 32;
constexpr uint32_t MIC_CALIBRATION_MS = 3000;
constexpr uint32_t MIC_EVENT_COOLDOWN_MS = 2500;
constexpr float MIC_AMBIENT_MULTIPLIER = 3.0f;
constexpr float MIC_AMBIENT_MARGIN = 18.0f;
constexpr float MIC_MINIMUM_THRESHOLD = 70.0f;

MicrophoneReading latestMicReading = { 0, 0, 0, 0, 0 };
uint32_t nextMicSampleAt = 0;
uint32_t micWindowStartedAt = 0;
uint32_t micSampleSum = 0;
uint16_t micSampleCount = 0;
uint16_t micSampleMinimum = UINT16_MAX;
uint16_t micSampleMaximum = 0;
uint32_t micDetectionWindowStartedAt = 0;
uint16_t micDetectionMinimum = UINT16_MAX;
uint16_t micDetectionMaximum = 0;
uint16_t latestMicDetectionAmplitude = 0;
float micAmbientAmplitude = 0.0f;
float latestMicDetectionThreshold = MIC_MINIMUM_THRESHOLD;
uint32_t micCalibrationStartedAt = 0;
uint32_t micCooldownUntil = 0;
bool micEventSinceLastReport = false;

// Backlight
int backlightCurrent = 255;
int backlightTarget = 255;

// Debounce
bool touchLastRaw = false;
bool touchStable = false;
uint32_t touchDebounceAt = 0;

bool buttonLastRaw = true;
bool buttonStable = true;
uint32_t buttonDebounceAt = 0;

// =========================
// Sound
// =========================
const uint16_t reactNotes[] = { 988, 1319, 1175, 1760, 1480, 1047, 1568 };
const uint16_t reactDurs[]  = {  65,   55,   55,   75,   65,   55,   85 };
const uint8_t REACT_COUNT = sizeof(reactNotes) / sizeof(reactNotes[0]);

void playSequenceBlocking(const uint16_t *notes, const uint16_t *durs, uint8_t count, uint16_t gapMs) {
  for (uint8_t i = 0; i < count; i++) {
    tone(BUZZER_PIN, notes[i], durs[i]);
    delay(durs[i] + gapMs);
  }
  noTone(BUZZER_PIN);
}

void playBootSound() {
  const uint16_t notes[] = { 880, 1320, 1760 };
  const uint16_t durs[]  = {  60,   60,   90 };
  playSequenceBlocking(notes, durs, 3, 20);
}

void playSleepSound() {
  const uint16_t notes[] = { 700, 560, 420 };
  const uint16_t durs[]  = {  80,  90, 110 };
  playSequenceBlocking(notes, durs, 3, 24);
}

void playWakeSound() {
  const uint16_t notes[] = { 660, 980, 1480 };
  const uint16_t durs[]  = {  60,  60,   90 };
  playSequenceBlocking(notes, durs, 3, 20);
}

// =========================
// Scheduling
// =========================
void scheduleNextBlink(uint32_t now) {
  nextBlinkAt = now + random(1800, 4200);
}

void scheduleNextLook(uint32_t now) {
  nextLookAt = now + random(600, 1600);
}

void scheduleNextDrowsy(uint32_t now) {
  nextDrowsyAt = now + random(7000, 14000);
}

// =========================
// Mode changes
// =========================
void triggerReaction(uint32_t now) {
  if (mode == MODE_SLEEP) return;

  mode = MODE_REACT;
  reactUntil = now + 1800;
  reactStep = 0;
  nextReactNoteAt = now;
  pupilTargetX = 0;
  pupilTargetY = 0;
  drowsyUntil = 0;
}

void toggleSleep(uint32_t now) {
  if (mode == MODE_SLEEP) {
    mode = MODE_IDLE;
    backlightTarget = 255;
    blinkActive = true;
    blinkStart = now;
    blinkDuration = 260;
    scheduleNextBlink(now);
    scheduleNextLook(now);
    scheduleNextDrowsy(now);
    playWakeSound();
  } else {
    mode = MODE_SLEEP;
    backlightTarget = 40;
    blinkActive = false;
    reactUntil = 0;
    noTone(BUZZER_PIN);
    playSleepSound();
  }
}

// =========================
// Input
// =========================
void updateInputs(uint32_t now) {
  // TTP223 usually idles LOW and goes HIGH on touch
  bool rawTouch = digitalRead(TOUCH_PIN) == HIGH;
  if (rawTouch != touchLastRaw) {
    touchLastRaw = rawTouch;
    touchDebounceAt = now;
  }

  if ((now - touchDebounceAt) > 25 && rawTouch != touchStable) {
    touchStable = rawTouch;
    if (touchStable) {
      triggerReaction(now);
    }
  }

  // Button uses INPUT_PULLUP, so pressed = LOW
  bool rawButton = digitalRead(BUTTON_PIN) == HIGH;
  if (rawButton != buttonLastRaw) {
    buttonLastRaw = rawButton;
    buttonDebounceAt = now;
  }

  if ((now - buttonDebounceAt) > 25 && rawButton != buttonStable) {
    buttonStable = rawButton;
    if (buttonStable == LOW) {
      toggleSleep(now);
    }
  }
}

// =========================
// Microphone input
// =========================
void updateMicrophoneNoiseFloor(uint16_t amplitude, bool calibrating) {
  if (micAmbientAmplitude == 0.0f) {
    micAmbientAmplitude = amplitude;
    return;
  }

  // Calibration converges quickly. Afterward, background changes are followed
  // slowly, while unusually loud windows have little influence on the floor.
  float alpha = calibrating ? 0.20f : 0.06f;
  if (!calibrating && amplitude > latestMicDetectionThreshold) alpha = 0.005f;
  micAmbientAmplitude += (float(amplitude) - micAmbientAmplitude) * alpha;
}

bool reactionBuzzerIsPlaying() {
  return mode == MODE_REACT && reactStep < REACT_COUNT;
}

void processMicrophoneDetectionWindow(uint32_t now, uint16_t amplitude) {
  latestMicDetectionAmplitude = amplitude;
  bool calibrating = (now - micCalibrationStartedAt) < MIC_CALIBRATION_MS;

  // Do not teach the ambient floor the Buddy's own reaction sound.
  if (calibrating || mode != MODE_REACT) {
    updateMicrophoneNoiseFloor(amplitude, calibrating);
  }

  latestMicDetectionThreshold = max(
      MIC_MINIMUM_THRESHOLD,
      micAmbientAmplitude * MIC_AMBIENT_MULTIPLIER + MIC_AMBIENT_MARGIN);

  bool cooldownFinished = (int32_t)(now - micCooldownUntil) >= 0;
  bool mayTrigger = !calibrating && mode == MODE_IDLE &&
                    !reactionBuzzerIsPlaying() && cooldownFinished;
  if (mayTrigger && amplitude >= latestMicDetectionThreshold) {
    micEventSinceLastReport = true;
    micCooldownUntil = now + MIC_EVENT_COOLDOWN_MS;
    triggerReaction(now);
  }
}

void updateMicrophone(uint32_t now) {
  uint32_t nowUs = micros();
  if ((int32_t)(nowUs - nextMicSampleAt) >= 0) {
    // Schedule from the actual read time so a busy frame never causes a burst
    // of catch-up ADC reads that could delay the rest of the application.
    nextMicSampleAt = nowUs + MIC_SAMPLE_INTERVAL_US;

    uint16_t sample = analogRead(MIC_PIN);
    micSampleSum += sample;
    micSampleCount++;
    if (sample < micSampleMinimum) micSampleMinimum = sample;
    if (sample > micSampleMaximum) micSampleMaximum = sample;
    if (sample < micDetectionMinimum) micDetectionMinimum = sample;
    if (sample > micDetectionMaximum) micDetectionMaximum = sample;
  }

  if ((now - micDetectionWindowStartedAt) >= MIC_DETECTION_WINDOW_MS &&
      micDetectionMinimum != UINT16_MAX) {
    processMicrophoneDetectionWindow(
        now, micDetectionMaximum - micDetectionMinimum);
    micDetectionWindowStartedAt = now;
    micDetectionMinimum = UINT16_MAX;
    micDetectionMaximum = 0;
  }

  if ((now - micWindowStartedAt) >= MIC_REPORT_INTERVAL_MS && micSampleCount > 0) {
    latestMicReading.average = micSampleSum / micSampleCount;
    latestMicReading.minimum = micSampleMinimum;
    latestMicReading.maximum = micSampleMaximum;
    latestMicReading.peakToPeak = micSampleMaximum - micSampleMinimum;
    latestMicReading.timestamp = now;

    Serial.print("MIC avg=");
    Serial.print(latestMicReading.average);
    Serial.print(" min=");
    Serial.print(latestMicReading.minimum);
    Serial.print(" max=");
    Serial.print(latestMicReading.maximum);
    Serial.print(" peak-to-peak=");
    Serial.print(latestMicReading.peakToPeak);
    Serial.print(" short-p2p=");
    Serial.print(latestMicDetectionAmplitude);
    Serial.print(" ambient=");
    Serial.print(micAmbientAmplitude, 1);
    Serial.print(" threshold=");
    Serial.print(latestMicDetectionThreshold, 1);
    if ((now - micCalibrationStartedAt) < MIC_CALIBRATION_MS) {
      Serial.print(" CALIBRATING");
    }
    if (micEventSinceLastReport) Serial.print(" EVENT");
    Serial.println();
    micEventSinceLastReport = false;

    micWindowStartedAt = now;
    micSampleSum = 0;
    micSampleCount = 0;
    micSampleMinimum = UINT16_MAX;
    micSampleMaximum = 0;
  }
}

// =========================
// Backlight
// =========================
void updateBacklight() {
  if (backlightCurrent == backlightTarget) return;

  int diff = backlightTarget - backlightCurrent;
  if (abs(diff) < 2) {
    backlightCurrent = backlightTarget;
  } else {
    backlightCurrent += diff / 4;
  }

  backlightCurrent = constrain(backlightCurrent, 0, 255);
  analogWrite(TFT_BL, backlightCurrent);
}

// =========================
// Sound updater
// =========================
void updateSound(uint32_t now) {
  if (mode != MODE_REACT) return;

  if (reactStep < REACT_COUNT && now >= nextReactNoteAt) {
    tone(BUZZER_PIN, reactNotes[reactStep], reactDurs[reactStep]);
    nextReactNoteAt = now + reactDurs[reactStep] + 18;
    reactStep++;
  }
}

// =========================
// Animation
// =========================
void updateAnimation(uint32_t now) {
  if (mode == MODE_REACT && now >= reactUntil) {
    mode = MODE_IDLE;
    scheduleNextBlink(now);
    scheduleNextLook(now);
    scheduleNextDrowsy(now);
  }

  if (mode == MODE_IDLE && !blinkActive && now >= nextBlinkAt) {
    blinkActive = true;
    blinkStart = now;
    blinkDuration = random(140, 220);
    scheduleNextBlink(now + blinkDuration);
  }

  if (mode == MODE_IDLE && now >= nextDrowsyAt) {
    drowsyUntil = now + random(1300, 2800);
    scheduleNextDrowsy(now);
  }

  float blinkAmt = 0.0f;
  if (blinkActive) {
    float p = float(now - blinkStart) / float(blinkDuration);
    if (p >= 1.0f) {
      blinkActive = false;
    } else {
      blinkAmt = sinf(p * 3.1415926f);
    }
  }

  float sleepyAmt = 0.0f;
  if (mode == MODE_IDLE && now < drowsyUntil) {
    sleepyAmt = 0.25f + 0.08f * (0.5f + 0.5f * sinf(now * 0.004f));
  }

  float targetLid = 0.0f;
  if (mode == MODE_SLEEP) {
    targetLid = 1.0f;
  } else if (mode == MODE_REACT) {
    targetLid = 0.0f;
  } else {
    targetLid = max(blinkAmt, sleepyAmt);
  }

  lidAmount = lerpf(lidAmount, targetLid, 0.22f);

  if (mode == MODE_REACT) {
    pupilTargetX = 0;
    pupilTargetY = 0;
  } else if (mode == MODE_SLEEP) {
    pupilTargetX = 0;
    pupilTargetY = 10;
  } else if (now >= nextLookAt) {
    pupilTargetX = random(-16, 17);
    pupilTargetY = random(-10, 13);

    if (now < drowsyUntil) {
      pupilTargetY = random(6, 14);
    }

    scheduleNextLook(now);
  }

  float follow = (mode == MODE_REACT) ? 0.22f : 0.08f;
  pupilX = lerpf(pupilX, pupilTargetX, follow);
  pupilY = lerpf(pupilY, pupilTargetY, follow);
}

// =========================
// Drawing
// =========================
uint16_t backgroundColorAt(int y) {
  float t = float(y) / float(SCREEN_H - 1);
  uint8_t r = uint8_t((1.0f - t) * 6  + t * 10);
  uint8_t g = uint8_t((1.0f - t) * 10 + t * 18);
  uint8_t b = uint8_t((1.0f - t) * 22 + t * 38);
  return rgb565(r, g, b);
}

void fillBackgroundRegion(GFXcanvas16 &target, int screenY, int width, int height) {
  for (int y = 0; y < height; y++) {
    target.drawFastHLine(0, y, width, backgroundColorAt(screenY + y));
  }
}

void drawStaticBackground() {
  for (int y = 0; y < SCREEN_H; y++) {
    tft.drawFastHLine(0, y, SCREEN_W, backgroundColorAt(y));
  }

  // subtle top dots
  for (int i = 0; i < 6; i++) {
    int x = 25 + i * 45;
    int y = 24 + (i % 2) * 8;
    tft.fillCircle(x, y, 2, COL_ACCENT);
  }

  // small bottom base
  tft.fillRoundRect((SCREEN_W / 2) - 16, SCREEN_H - 22, 32, 8, 4, COL_PANEL);
}

void drawSpark(GFXcanvas16 &target, int x, int y, int len, uint16_t c) {
  target.drawLine(x - len, y, x + len, y, c);
  target.drawLine(x, y - len, x, y + len, c);
  target.drawLine(x - len / 2, y - len / 2, x + len / 2, y + len / 2, c);
  target.drawLine(x - len / 2, y + len / 2, x + len / 2, y - len / 2, c);
}

void drawEye(GFXcanvas16 &target, int cx, int cy, int w, int h, float lid, bool reacting) {
  int x = cx - (w / 2);
  int y = cy - (h / 2);

  // fully closed eye
  if (lid > 0.93f) {
    target.fillRoundRect(x + 10, cy - 2, w - 20, 5, 2, COL_EYE_LINE);
    target.drawFastHLine(cx - 18, cy + 5, 36, COL_HILITE);
    return;
  }

  // eye body
  target.fillRoundRect(x, y, w, h, eyeCorner, COL_EYE_WHITE);
  target.drawRoundRect(x, y, w, h, eyeCorner, COL_EYE_LINE);

  // pupil limits
  int pupilRangeX = (w / 2) - 22;
  int pupilRangeY = (h / 2) - 24;

  int px = cx + int(clampf(pupilX, -pupilRangeX, pupilRangeX));
  int py = cy + int(clampf(pupilY, -pupilRangeY, pupilRangeY));

  uint16_t irisColor = reacting ? COL_IRIS_REACT : COL_IRIS;
  int irisR = reacting ? 17 : 18;
  int pupilR = reacting ? 7 : 8;

  target.fillCircle(px, py, irisR, irisColor);
  target.fillCircle(px, py, pupilR, COL_PUPIL);
  target.fillCircle(px - 5, py - 5, 4, COL_HILITE);
  target.fillCircle(px + 6, py + 6, 2, COL_HILITE);

  // lower accent
  target.drawFastHLine(x + 16, y + h - 12, w - 32, COL_HILITE);

  // lids cover eye from top/bottom
  int cover = int((h / 2.0f) * lid);

  if (cover > 0) {
    // top lid
    target.fillRoundRect(x - 1, y - 1, w + 2, cover + 4, eyeCorner, COL_BG_TOP);
    // bottom lid
    target.fillRoundRect(x - 1, y + h - cover - 3, w + 2, cover + 5, eyeCorner, COL_BG_BOTTOM);

    // lid edge line
    target.drawFastHLine(x + 10, y + cover, w - 20, COL_EYE_LINE);
    target.drawFastHLine(x + 10, y + h - cover, w - 20, COL_EYE_LINE);
  }
}

void renderEyeRegion(int screenCenterX, int screenCenterY,
                     int sparkCenterX, bool reacting) {
  int screenX = screenCenterX - (EYE_REGION_W / 2);
  fillBackgroundRegion(eyeCanvas, EYE_REGION_Y, EYE_REGION_W, EYE_REGION_H);
  drawEye(eyeCanvas, EYE_REGION_W / 2, screenCenterY - EYE_REGION_Y,
          eyeW, eyeH, lidAmount, reacting);
  if (reacting) {
    drawSpark(eyeCanvas, sparkCenterX - screenX,
              screenCenterY - 56 - EYE_REGION_Y, 5, COL_IRIS_REACT);
  }
  tft.drawRGBBitmap(screenX, EYE_REGION_Y, eyeCanvas.getBuffer(),
                    EYE_REGION_W, EYE_REGION_H);
}

void renderSleepZ(uint32_t now) {
  constexpr int Z_X = 212;
  constexpr int Z_Y = 31;
  fillBackgroundRegion(effectCanvas, Z_Y, EFFECT_REGION_W, EFFECT_REGION_H);
  if (mode == MODE_SLEEP) {
    effectCanvas.setTextColor(COL_SLEEP);
    effectCanvas.setTextSize(2);
    effectCanvas.setCursor(0, 34 + int(sinf(now * 0.003f) * 3.0f) - Z_Y);
    effectCanvas.print("Z");
  }
  tft.drawRGBBitmap(Z_X, Z_Y, effectCanvas.getBuffer(),
                    EFFECT_REGION_W, EFFECT_REGION_H);
}

void renderFrame(uint32_t now) {
  int bobY;
  if (mode == MODE_SLEEP) {
    bobY = int(sinf(now * 0.0022f) * 3.0f);
  } else {
    bobY = int(sinf(now * 0.0045f) * 1.5f);
  }

  int y = eyeY + bobY;
  bool reacting = (mode == MODE_REACT);

  renderEyeRegion(leftEyeX, y, leftEyeX - 34, reacting);
  renderEyeRegion(rightEyeX, y, rightEyeX + 34, reacting);
  renderSleepZ(now);
}

// =========================
// Setup / loop
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MIC_PIN, INPUT);
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

  // If your colors look like a photo negative, uncomment this:
  // tft.invertDisplay(true);

  randomSeed(micros());

  scheduleNextBlink(millis());
  scheduleNextLook(millis());
  scheduleNextDrowsy(millis());

  nextMicSampleAt = micros();
  micWindowStartedAt = millis();
  micDetectionWindowStartedAt = micWindowStartedAt;
  Serial.println("MAX4466 microphone enabled on GP26 / ADC0");

  playBootSound();
  // Start calibration after the boot tones so they cannot establish the floor.
  micCalibrationStartedAt = millis();
  micDetectionWindowStartedAt = micCalibrationStartedAt;
}

void loop() {
  uint32_t now = millis();

  updateMicrophone(now);
  updateInputs(now);
  updateAnimation(now);
  updateSound(now);
  updateBacklight();

  // Target ~20 FPS, updating only the buffered animated regions.
  if (now - lastFrameAt >= 50) {
    lastFrameAt = now;
    renderFrame(now);
  }
}

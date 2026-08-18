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

// =========================
// Display
// =========================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// 1.69" screen in rotation 1 = 280 x 240
constexpr int SCREEN_W = 280;
constexpr int SCREEN_H = 240;

// Full-screen 16-bit canvas for smooth drawing
GFXcanvas16 canvas(SCREEN_W, SCREEN_H);

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
void drawGradientBackground() {
  for (int y = 0; y < SCREEN_H; y++) {
    float t = float(y) / float(SCREEN_H - 1);
    uint8_t r = uint8_t((1.0f - t) * 6  + t * 10);
    uint8_t g = uint8_t((1.0f - t) * 10 + t * 18);
    uint8_t b = uint8_t((1.0f - t) * 22 + t * 38);
    canvas.drawFastHLine(0, y, SCREEN_W, rgb565(r, g, b));
  }

  // subtle top dots
  for (int i = 0; i < 6; i++) {
    int x = 25 + i * 45;
    int y = 24 + (i % 2) * 8;
    canvas.fillCircle(x, y, 2, COL_ACCENT);
  }

  // small bottom base
  canvas.fillRoundRect((SCREEN_W / 2) - 16, SCREEN_H - 22, 32, 8, 4, COL_PANEL);
}

void drawSpark(int x, int y, int len, uint16_t c) {
  canvas.drawLine(x - len, y, x + len, y, c);
  canvas.drawLine(x, y - len, x, y + len, c);
  canvas.drawLine(x - len / 2, y - len / 2, x + len / 2, y + len / 2, c);
  canvas.drawLine(x - len / 2, y + len / 2, x + len / 2, y - len / 2, c);
}

void drawEye(int cx, int cy, int w, int h, float lid, bool reacting) {
  int x = cx - (w / 2);
  int y = cy - (h / 2);

  // fully closed eye
  if (lid > 0.93f) {
    canvas.fillRoundRect(x + 10, cy - 2, w - 20, 5, 2, COL_EYE_LINE);
    canvas.drawFastHLine(cx - 18, cy + 5, 36, COL_HILITE);
    return;
  }

  // eye body
  canvas.fillRoundRect(x, y, w, h, eyeCorner, COL_EYE_WHITE);
  canvas.drawRoundRect(x, y, w, h, eyeCorner, COL_EYE_LINE);

  // pupil limits
  int pupilRangeX = (w / 2) - 22;
  int pupilRangeY = (h / 2) - 24;

  int px = cx + int(clampf(pupilX, -pupilRangeX, pupilRangeX));
  int py = cy + int(clampf(pupilY, -pupilRangeY, pupilRangeY));

  uint16_t irisColor = reacting ? COL_IRIS_REACT : COL_IRIS;
  int irisR = reacting ? 17 : 18;
  int pupilR = reacting ? 7 : 8;

  canvas.fillCircle(px, py, irisR, irisColor);
  canvas.fillCircle(px, py, pupilR, COL_PUPIL);
  canvas.fillCircle(px - 5, py - 5, 4, COL_HILITE);
  canvas.fillCircle(px + 6, py + 6, 2, COL_HILITE);

  // lower accent
  canvas.drawFastHLine(x + 16, y + h - 12, w - 32, COL_HILITE);

  // lids cover eye from top/bottom
  int cover = int((h / 2.0f) * lid);

  if (cover > 0) {
    // top lid
    canvas.fillRoundRect(x - 1, y - 1, w + 2, cover + 4, eyeCorner, COL_BG_TOP);
    // bottom lid
    canvas.fillRoundRect(x - 1, y + h - cover - 3, w + 2, cover + 5, eyeCorner, COL_BG_BOTTOM);

    // lid edge line
    canvas.drawFastHLine(x + 10, y + cover, w - 20, COL_EYE_LINE);
    canvas.drawFastHLine(x + 10, y + h - cover, w - 20, COL_EYE_LINE);
  }
}

void renderFrame(uint32_t now) {
  canvas.fillScreen(0);
  drawGradientBackground();

  int bobY;
  if (mode == MODE_SLEEP) {
    bobY = int(sinf(now * 0.0022f) * 3.0f);
  } else {
    bobY = int(sinf(now * 0.0045f) * 1.5f);
  }

  int y = eyeY + bobY;
  bool reacting = (mode == MODE_REACT);

  drawEye(leftEyeX, y, eyeW, eyeH, lidAmount, reacting);
  drawEye(rightEyeX, y, eyeW, eyeH, lidAmount, reacting);

  if (reacting) {
    drawSpark(leftEyeX - 34, y - 56, 5, COL_IRIS_REACT);
    drawSpark(rightEyeX + 34, y - 56, 5, COL_IRIS_REACT);
  }

  if (mode == MODE_SLEEP) {
    canvas.setTextColor(COL_SLEEP);
    canvas.setTextSize(2);
    canvas.setCursor(212, 34 + int(sinf(now * 0.003f) * 3.0f));
    canvas.print("Z");
  }

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_W, SCREEN_H);
}

// =========================
// Setup / loop
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TFT_BL, OUTPUT);

  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(TFT_BL, 255);

  SPI.setSCK(TFT_SCLK);
  SPI.setTX(TFT_MOSI);
  SPI.begin();

  tft.init(240, 280);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  // If your colors look like a photo negative, uncomment this:
  // tft.invertDisplay(true);

  randomSeed(micros());

  scheduleNextBlink(millis());
  scheduleNextLook(millis());
  scheduleNextDrowsy(millis());

  playBootSound();
}

void loop() {
  uint32_t now = millis();

  updateInputs(now);
  updateAnimation(now);
  updateSound(now);
  updateBacklight();

  // Target ~20 FPS. Stable and light enough for full-screen SPI updates.
  if (now - lastFrameAt >= 50) {
    lastFrameAt = now;
    renderFrame(now);
  }
}
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

#define TOUCH_PIN   5    // TTP223 OUT -> GP5; VCC -> VBUS; GND -> Pico GND
#define BUTTON_PIN  7    // push button to GND, use INPUT_PULLUP
#define BUZZER_PIN  15   // passive buzzer signal
#define SOUND_SENSOR_PIN 26  // VKLSVAN OUT -> GP26; VCC -> 3V3; GND -> Pico GND

// The VKLSVAN digital output idles HIGH and goes LOW when sound exceeds the
// threshold set by its onboard blue potentiometer.
constexpr uint8_t SOUND_SENSOR_ACTIVE_LEVEL = LOW;
static_assert(SOUND_SENSOR_ACTIVE_LEVEL == LOW, "VKLSVAN trigger must be active LOW");

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
// Buddy behavior state
// =========================
enum class BuddyCoreState : uint8_t {
  Awake,
  Sleeping
};

enum class BuddyEvent : uint8_t {
  Touch,
  SoundDetected,
  Wake,
  SleepRequested
};

enum class BuddyReaction : uint8_t {
  Idle,
  Generic
};

BuddyCoreState coreState = BuddyCoreState::Awake;
BuddyReaction activeReaction = BuddyReaction::Idle;

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
uint32_t reactNoteEndsAt = 0;
uint8_t reactStep = 0;
bool reactNotePlaying = false;

uint32_t lastFrameAt = 0;

// Sound sensor event capture
constexpr uint32_t SOUND_EVENT_COOLDOWN_MS = 2500;
constexpr uint32_t SOUND_STARTUP_IGNORE_MS = 250;
volatile bool soundActivationPending = false;
uint32_t soundCooldownUntil = 0;
uint32_t soundIgnoreUntil = 0;

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
    // Stop each note ourselves rather than relying on tone()'s duration timer.
    // This also guarantees a silent gap before the next frequency is started.
    tone(BUZZER_PIN, notes[i]);
    delay(durs[i]);
    noTone(BUZZER_PIN);
    delay(gapMs);
  }
  noTone(BUZZER_PIN);
}

void stopReactionSound() {
  noTone(BUZZER_PIN);
  reactNotePlaying = false;
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
// Behavior engine
// =========================
void startGenericReaction(uint32_t now) {
  stopReactionSound();
  activeReaction = BuddyReaction::Generic;
  reactUntil = now + 1800;
  reactStep = 0;
  nextReactNoteAt = now;
  pupilTargetX = 0;
  pupilTargetY = 0;
  drowsyUntil = 0;
}

void enterSleep(uint32_t now) {
  coreState = BuddyCoreState::Sleeping;
  activeReaction = BuddyReaction::Idle;
  backlightTarget = 40;
  blinkActive = false;
  reactUntil = 0;
  stopReactionSound();
  playSleepSound();
}

void wakeBuddy(uint32_t now) {
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  backlightTarget = 255;
  blinkActive = true;
  blinkStart = now;
  blinkDuration = 260;
  scheduleNextBlink(now);
  scheduleNextLook(now);
  scheduleNextDrowsy(now);
  playWakeSound();
  // Ignore any edge captured from the blocking wake tones.
  soundIgnoreUntil = millis() + SOUND_STARTUP_IGNORE_MS;
}

void processBuddyEvent(BuddyEvent event, uint32_t now) {
  switch (event) {
    case BuddyEvent::SleepRequested:
      if (coreState == BuddyCoreState::Awake) enterSleep(now);
      break;
    case BuddyEvent::Wake:
      if (coreState == BuddyCoreState::Sleeping) wakeBuddy(now);
      break;
    case BuddyEvent::Touch:
    case BuddyEvent::SoundDetected:
      if (coreState == BuddyCoreState::Awake) startGenericReaction(now);
      break;
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
      processBuddyEvent(BuddyEvent::Touch, now);
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
      BuddyEvent event = (coreState == BuddyCoreState::Sleeping)
          ? BuddyEvent::Wake : BuddyEvent::SleepRequested;
      processBuddyEvent(event, now);
    }
  }
}

// =========================
// Digital sound sensor input
// =========================
void captureSoundActivation() {
  // Keep the ISR minimal. The main loop applies all state and cooldown rules.
  soundActivationPending = true;
}

void updateSoundSensor(uint32_t now) {
  noInterrupts();
  bool activationCaptured = soundActivationPending;
  soundActivationPending = false;
  interrupts();

  if (!activationCaptured) return;

  // Always consume captured edges. Ineligible activations (including the
  // Buddy's own tones) are discarded instead of being deferred.
  bool ignored = (int32_t)(now - soundIgnoreUntil) < 0;
  bool coolingDown = (int32_t)(now - soundCooldownUntil) < 0;
  bool reactionActive = activeReaction != BuddyReaction::Idle || reactNotePlaying;
  if (ignored || coolingDown || reactionActive ||
      coreState == BuddyCoreState::Sleeping) return;

  Serial.println("SOUND EVENT");
  soundCooldownUntil = now + SOUND_EVENT_COOLDOWN_MS;
  processBuddyEvent(BuddyEvent::SoundDetected, now);
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
  if (activeReaction != BuddyReaction::Generic) {
    if (reactNotePlaying) stopReactionSound();
    return;
  }

  if (reactNotePlaying) {
    if ((int32_t)(now - reactNoteEndsAt) < 0) return;

    noTone(BUZZER_PIN);
    reactNotePlaying = false;
    nextReactNoteAt = now + 18;
    return;
  }

  if (reactStep < REACT_COUNT && (int32_t)(now - nextReactNoteAt) >= 0) {
    tone(BUZZER_PIN, reactNotes[reactStep]);
    reactNoteEndsAt = now + reactDurs[reactStep];
    reactNotePlaying = true;
    reactStep++;
  }
}

// =========================
// Animation
// =========================
void updateAnimation(uint32_t now) {
  if (activeReaction == BuddyReaction::Generic && now >= reactUntil) {
    activeReaction = BuddyReaction::Idle;
    scheduleNextBlink(now);
    scheduleNextLook(now);
    scheduleNextDrowsy(now);
  }

  if (coreState == BuddyCoreState::Awake &&
      activeReaction == BuddyReaction::Idle && !blinkActive && now >= nextBlinkAt) {
    blinkActive = true;
    blinkStart = now;
    blinkDuration = random(140, 220);
    scheduleNextBlink(now + blinkDuration);
  }

  if (coreState == BuddyCoreState::Awake &&
      activeReaction == BuddyReaction::Idle && now >= nextDrowsyAt) {
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
  if (coreState == BuddyCoreState::Awake &&
      activeReaction == BuddyReaction::Idle && now < drowsyUntil) {
    sleepyAmt = 0.25f + 0.08f * (0.5f + 0.5f * sinf(now * 0.004f));
  }

  float targetLid = 0.0f;
  if (coreState == BuddyCoreState::Sleeping) {
    targetLid = 1.0f;
  } else if (activeReaction == BuddyReaction::Generic) {
    targetLid = 0.0f;
  } else {
    targetLid = max(blinkAmt, sleepyAmt);
  }

  lidAmount = lerpf(lidAmount, targetLid, 0.22f);

  if (activeReaction == BuddyReaction::Generic) {
    pupilTargetX = 0;
    pupilTargetY = 0;
  } else if (coreState == BuddyCoreState::Sleeping) {
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

  float follow = (activeReaction == BuddyReaction::Generic) ? 0.22f : 0.08f;
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
  if (coreState == BuddyCoreState::Sleeping) {
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
  if (coreState == BuddyCoreState::Sleeping) {
    bobY = int(sinf(now * 0.0022f) * 3.0f);
  } else {
    bobY = int(sinf(now * 0.0045f) * 1.5f);
  }

  int y = eyeY + bobY;
  bool reacting = (activeReaction == BuddyReaction::Generic);

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
  pinMode(SOUND_SENSOR_PIN, INPUT);
  pinMode(TFT_BL, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_SENSOR_PIN), captureSoundActivation, FALLING);

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

  Serial.println("VKLSVAN sound sensor ready on GP26 (VCC: 3V3)");
  Serial.println("Trigger level: ACTIVE LOW (HIGH = idle, LOW = sound)");
  Serial.println("TTP223 touch enabled on GP5 (VCC: VBUS)");

  playBootSound();
  // Discard edges from boot audio and briefly ignore subsequent startup noise.
  noInterrupts();
  soundActivationPending = false;
  interrupts();
  soundIgnoreUntil = millis() + SOUND_STARTUP_IGNORE_MS;
}

void loop() {
  uint32_t now = millis();

  updateSoundSensor(now);
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

#include "BuddyLifeSimulator.h"

#include <deque>

namespace {
uint32_t simulatedNow = 0;
constexpr uint32_t FACE_DURATION_MS = 1800;
// Host-only activity approximation, NOT exact buzzer sequence duration.
// Reaction and system cues use 600 ms solely for engine eligibility checks.
constexpr uint32_t SOUND_ACTIVITY_MS = 600;
uint32_t faceStartedAt = 0;
bool faceActive = false;
FaceExpression lastExpression = FaceExpression::Normal;
uint32_t soundStartedAt = 0;
bool reactionAudio = false;
bool systemAudio = false;
ReactionSound lastSound = ReactionSound::None;
BuddyMood lastSoundMood = BuddyMood::Calm;
std::deque<uint32_t> randomTickets;
void systemCue() {
  soundStartedAt = simulatedNow;
  systemAudio = true;
  reactionAudio = false;
}
}

uint32_t millis() { return simulatedNow; }
long random(long maximum) {
  if (maximum <= 0) return 0;
  if (randomTickets.empty()) return 0;
  const uint32_t ticket = randomTickets.front();
  randomTickets.pop_front();
  return static_cast<long>(ticket % static_cast<unsigned long>(maximum));
}
long random(long minimum, long maximum) {
  return maximum <= minimum ? minimum : minimum + random(maximum - minimum);
}
void scheduleFaceBehavior(uint32_t) {}
void startFaceReaction(uint32_t now, FaceExpression expression) {
  lastExpression = expression;
  faceStartedAt = now;
  faceActive = true;
}
bool isFaceReactionFinished(uint32_t now) {
  return faceActive && static_cast<uint32_t>(now - faceStartedAt) >= FACE_DURATION_MS;
}
void finishFaceReaction(uint32_t) { faceActive = false; }
void enterSleepFace(uint32_t) { faceActive = false; }
void wakeFace(uint32_t) { faceActive = false; }
void ignoreSoundSensorAfterWake() {}
void startReactionSound(uint32_t now, ReactionSound sound, BuddyMood mood) {
  lastSound = sound;
  lastSoundMood = mood;
  soundStartedAt = now;
  reactionAudio = sound != ReactionSound::None;
  systemAudio = false;
}
void stopReactionSound() { reactionAudio = false; }
bool isSoundEngineActive() {
  return (reactionAudio || systemAudio) &&
      static_cast<uint32_t>(simulatedNow - soundStartedAt) < SOUND_ACTIVITY_MS;
}
void playBootSound() { systemCue(); }
void playSleepSound() { systemCue(); }
void playWakeSound() { systemCue(); }
bool startDiagnosticReactionSound(uint32_t now, ReactionSound sound,
                                  uint8_t requested, uint8_t &selected, BuddyMood mood) {
  if (requested != DIAGNOSTIC_RANDOM_VARIANT && requested > 2) return false;
  selected = requested == DIAGNOSTIC_RANDOM_VARIANT ? 0 : requested;
  startReactionSound(now, sound, mood);
  return true;
}

void BuddyLifeSimulator::reset(uint32_t startAt) {
  simulatedNow = startAt;
  faceStartedAt = soundStartedAt = startAt;
  faceActive = reactionAudio = systemAudio = false;
  lastExpression = FaceExpression::Normal;
  lastSound = ReactionSound::None;
  lastSoundMood = BuddyMood::Calm;
  clearRandomQueue();
  beginBehaviorEngine(simulatedNow);
  observe("Reset");
}
void BuddyLifeSimulator::advanceBy(uint32_t milliseconds) {
  if (milliseconds == 0) {
    updateBehaviorEngine(simulatedNow);
    observe("Tick");
    return;
  }
  while (milliseconds > 0) {
    const uint32_t step = milliseconds < TICK_MS ? milliseconds : TICK_MS;
    simulatedNow += step;
    milliseconds -= step;
    updateBehaviorEngine(simulatedNow);
    observe("Tick");
  }
}
void BuddyLifeSimulator::advanceTo(uint32_t timestamp) {
  advanceBy(static_cast<uint32_t>(timestamp - simulatedNow));
}
void BuddyLifeSimulator::event(BuddyEvent value) {
  processBuddyEvent(value, simulatedNow);
  switch (value) {
    case BuddyEvent::TouchTap: observe("Tap"); break;
    case BuddyEvent::TouchHold: observe("Hold"); break;
    case BuddyEvent::SoundDetected: observe("Sound"); break;
    case BuddyEvent::ButtonShortPress: observe("ShortButtonPress"); break;
    case BuddyEvent::ButtonLongPress: observe("LongButtonPress"); break;
    case BuddyEvent::IdleTimeout: observe("Autonomous"); break;
  }
}
void BuddyLifeSimulator::tap() { event(BuddyEvent::TouchTap); }
void BuddyLifeSimulator::hold() { event(BuddyEvent::TouchHold); }
void BuddyLifeSimulator::sound() { event(BuddyEvent::SoundDetected); }
void BuddyLifeSimulator::shortButtonPress() { event(BuddyEvent::ButtonShortPress); }
void BuddyLifeSimulator::longButtonPress() { event(BuddyEvent::ButtonLongPress); }
void BuddyLifeSimulator::autonomousEvent() { event(BuddyEvent::IdleTimeout); }
uint32_t BuddyLifeSimulator::now() const { return simulatedNow; }
BuddyLifeSnapshot BuddyLifeSimulator::snapshot() const {
  const auto mood = getDiagnosticMoodState(simulatedNow);
  const auto memory = getDiagnosticRecentInteractionContext(simulatedNow);
  return {simulatedNow, getBuddyCoreState(), getBuddyReaction(), mood.mood,
          getBuddySoundMode(), mood.engagementScore, mood.irritationScore,
          mood.inactivityMs, memory.type, memory.ageMs, memory.recent,
          lastExpression, lastSound, lastSoundMood, isSoundEngineActive()};
}
void BuddyLifeSimulator::queueRandom(uint32_t value) { randomTickets.push_back(value); }
void BuddyLifeSimulator::clearRandomQueue() { randomTickets.clear(); }

void BuddyLifeSimulator::observe(const char* label) const {
  if (observer_ != nullptr) observer_(snapshot(), label, observerContext_);
}
void BuddyLifeSimulator::setObserver(BuddyLifeObserver observer, void* context) {
  observer_ = observer;
  observerContext_ = context;
}
bool BuddyLifeSimulator::runScript(const ScheduledBuddyEvent* events, size_t count,
                                   uint32_t endAt) {
  if (events == nullptr && count != 0) return false;
  uint32_t previous = 0;
  for (size_t index = 0; index < count; ++index) {
    if (events[index].atMs < previous || events[index].atMs > endAt) return false;
    previous = events[index].atMs;
    switch (events[index].event) {
      case SimulatedBuddyEvent::Tap:
      case SimulatedBuddyEvent::Hold:
      case SimulatedBuddyEvent::Sound:
      case SimulatedBuddyEvent::ShortButtonPress:
      case SimulatedBuddyEvent::LongButtonPress:
      case SimulatedBuddyEvent::Autonomous:
      case SimulatedBuddyEvent::Checkpoint: break;
      default: return false;
    }
  }
  const uint32_t startedAt = now();
  for (size_t index = 0; index < count; ++index) {
    advanceTo(static_cast<uint32_t>(startedAt + events[index].atMs));
    switch (events[index].event) {
      case SimulatedBuddyEvent::Tap: tap(); break;
      case SimulatedBuddyEvent::Hold: hold(); break;
      case SimulatedBuddyEvent::Sound: sound(); break;
      case SimulatedBuddyEvent::ShortButtonPress: shortButtonPress(); break;
      case SimulatedBuddyEvent::LongButtonPress: longButtonPress(); break;
      case SimulatedBuddyEvent::Autonomous: autonomousEvent(); break;
      case SimulatedBuddyEvent::Checkpoint: observe("Checkpoint"); break;
    }
  }
  advanceTo(static_cast<uint32_t>(startedAt + endAt));
  return true;
}

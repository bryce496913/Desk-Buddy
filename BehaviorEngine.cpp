#include "BehaviorEngine.h"

#include "FaceRenderer.h"
#include "SoundEngine.h"
#include "SoundSensor.h"

namespace {
constexpr uint32_t TOUCH_REPEAT_WINDOW_MS = 6000;
constexpr uint32_t SOUND_REPEAT_WINDOW_MS = 10000;
constexpr uint32_t IDLE_PERSONALITY_MIN_MS = 20000;
constexpr uint32_t IDLE_PERSONALITY_MAX_MS = 40001;
constexpr uint8_t MAX_MOOD_SCORE = 100;
constexpr uint8_t ENGAGED_THRESHOLD = 40;
constexpr uint8_t GRUMPY_THRESHOLD = 60;
constexpr uint32_t MOOD_DECAY_INTERVAL_MS = 10000;
constexpr uint32_t SLEEPY_AFTER_INACTIVITY_MS = 90000;

enum class IdlePersonality : uint8_t {
  Curious,
  Daydreaming
};

struct ReactionPlan {
  FaceExpression expression;
  ReactionSound sound;
};

ReactionPlan selectTouchReaction(BuddyMood mood, uint8_t streak) {
  (void)mood;  // Pass 3A keeps the V1 mapping for every mood.
  if (streak == 1) {
    return {FaceExpression::Happy, ReactionSound::Happy};
  }
  if (streak == 2) {
    return {FaceExpression::Curious, ReactionSound::Curious};
  }
  return {FaceExpression::Annoyed, ReactionSound::Annoyed};
}

ReactionPlan selectSoundReaction(BuddyMood mood, uint8_t streak) {
  (void)mood;  // Pass 3A keeps the V1 mapping for every mood.
  if (streak == 1) {
    return {FaceExpression::Startled, ReactionSound::Startled};
  }
  if (streak == 2) {
    return {FaceExpression::Suspicious, ReactionSound::Suspicious};
  }
  return {FaceExpression::Confused, ReactionSound::Confused};
}

BuddyCoreState coreState = BuddyCoreState::Awake;
BuddyReaction activeReaction = BuddyReaction::Idle;
BuddyMood currentMood = BuddyMood::Calm;
uint8_t engagementScore = 0;
uint8_t irritationScore = 0;
uint32_t lastMoodDecayAt = 0;
uint32_t lastMeaningfulActivityAt = 0;
uint32_t lastTouchAt = 0;
uint8_t touchStreak = 0;
uint32_t lastSoundAt = 0;
uint8_t soundStreak = 0;
uint32_t nextIdlePersonalityAt = 0;
bool idlePersonalityScheduled = false;
bool autonomousReactionActive = false;
IdlePersonality lastIdlePersonality = IdlePersonality::Curious;
bool hasLastIdlePersonality = false;

void increaseScore(uint8_t &score, uint8_t amount) {
  const uint16_t increased = static_cast<uint16_t>(score) + amount;
  score = increased > MAX_MOOD_SCORE ? MAX_MOOD_SCORE
                                    : static_cast<uint8_t>(increased);
}

void updateMoodState(uint32_t now) {
  if (irritationScore >= GRUMPY_THRESHOLD) {
    currentMood = BuddyMood::Grumpy;
  } else if (engagementScore >= ENGAGED_THRESHOLD) {
    currentMood = BuddyMood::Engaged;
  } else if (coreState == BuddyCoreState::Awake &&
             activeReaction == BuddyReaction::Idle && !isSoundEngineActive() &&
             static_cast<uint32_t>(now - lastMeaningfulActivityAt) >=
                 SLEEPY_AFTER_INACTIVITY_MS) {
    currentMood = BuddyMood::Sleepy;
  } else {
    currentMood = BuddyMood::Calm;
  }
}

void decreaseScore(uint8_t &score, uint32_t amount) {
  score = amount >= score ? 0 : static_cast<uint8_t>(score - amount);
}

bool decayMoodScores(uint32_t now) {
  const uint32_t steps =
      static_cast<uint32_t>(now - lastMoodDecayAt) / MOOD_DECAY_INTERVAL_MS;
  if (steps == 0) return false;

  // Even a full uint32_t elapsed interval fits these products. Preserve the
  // fractional interval so delayed updates do not move the decay schedule.
  decreaseScore(engagementScore, steps * 5);
  decreaseScore(irritationScore, steps * 10);
  lastMoodDecayAt += steps * MOOD_DECAY_INTERVAL_MS;
  return true;
}

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void scheduleNextIdlePersonality(uint32_t now) {
#if DESK_BUDDY_DIAGNOSTICS
  (void)now;
  nextIdlePersonalityAt = 0;
  idlePersonalityScheduled = false;
#else
  nextIdlePersonalityAt =
      now + static_cast<uint32_t>(
                random(IDLE_PERSONALITY_MIN_MS, IDLE_PERSONALITY_MAX_MS));
  idlePersonalityScheduled = true;
#endif
}

void disableIdlePersonality() {
  nextIdlePersonalityAt = 0;
  idlePersonalityScheduled = false;
  autonomousReactionActive = false;
}

IdlePersonality selectIdlePersonality() {
  IdlePersonality selected = static_cast<IdlePersonality>(random(2));
  if (hasLastIdlePersonality && selected == lastIdlePersonality) {
    selected = selected == IdlePersonality::Curious
        ? IdlePersonality::Daydreaming
        : IdlePersonality::Curious;
  }
  lastIdlePersonality = selected;
  hasLastIdlePersonality = true;
  return selected;
}

FaceExpression faceExpressionFor(IdlePersonality personality) {
  return personality == IdlePersonality::Curious
      ? FaceExpression::Curious
      : FaceExpression::Daydreaming;
}

void resetTouchHistory() {
  lastTouchAt = 0;
  touchStreak = 0;
}

void resetSoundHistory() {
  lastSoundAt = 0;
  soundStreak = 0;
}

void startGenericReaction(uint32_t now, FaceExpression expression,
                          ReactionSound sound) {
  stopReactionSound();
  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now, expression);
  startReactionSound(now, sound);
}

void enterSleep(uint32_t now) {
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = now;
  coreState = BuddyCoreState::Sleeping;
  activeReaction = BuddyReaction::Idle;
  resetTouchHistory();
  resetSoundHistory();
  disableIdlePersonality();
  enterSleepFace(now);
  stopReactionSound();
  playSleepSound();
}

void wakeBuddy(uint32_t now) {
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = now;
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  resetSoundHistory();
  autonomousReactionActive = false;
  scheduleNextIdlePersonality(now);
  wakeFace(now);
  playWakeSound();
  ignoreSoundSensorAfterWake();
  updateMoodState(now);
}
}  // namespace

void beginBehaviorEngine(uint32_t now) {
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  currentMood = BuddyMood::Calm;
  engagementScore = 0;
  irritationScore = 0;
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = now;
  resetTouchHistory();
  resetSoundHistory();
  scheduleFaceBehavior(now);
  autonomousReactionActive = false;
  scheduleNextIdlePersonality(now);
}

void updateBehaviorEngine(uint32_t now) {
  const bool scoresDecayed =
      coreState == BuddyCoreState::Awake && decayMoodScores(now);

  if (activeReaction == BuddyReaction::Generic && isFaceReactionFinished(now)) {
    activeReaction = BuddyReaction::Idle;
    finishFaceReaction(now);
    if (autonomousReactionActive) {
      autonomousReactionActive = false;
      scheduleNextIdlePersonality(now);
    }
  }

  if (coreState == BuddyCoreState::Awake && idlePersonalityScheduled &&
      timeReached(now, nextIdlePersonalityAt)) {
    if (activeReaction == BuddyReaction::Idle && !isSoundEngineActive()) {
      idlePersonalityScheduled = false;
      processBuddyEvent(BuddyEvent::IdleTimeout, now);
    } else {
      scheduleNextIdlePersonality(now);
    }
  }

  // Evaluate after completion/autonomy so Sleepy requires actual eligibility.
  // Before inactivity expires, a forced diagnostic mood lasts until scores
  // change, matching the existing diagnostic selection semantics.
  if (coreState == BuddyCoreState::Awake &&
      (scoresDecayed || static_cast<uint32_t>(now - lastMeaningfulActivityAt) >=
                            SLEEPY_AFTER_INACTIVITY_MS)) {
    updateMoodState(now);
  }
}

void processBuddyEvent(BuddyEvent event, uint32_t now) {
  switch (event) {
    case BuddyEvent::ButtonPressed:
      if (coreState == BuddyCoreState::Sleeping) {
        wakeBuddy(now);
      } else {
        enterSleep(now);
      }
      break;
    case BuddyEvent::Touch:
      if (coreState == BuddyCoreState::Awake) {
        // Selection uses arrival context, before this touch changes mood.
        const BuddyMood reactionMood = currentMood;
        lastMeaningfulActivityAt = now;
        autonomousReactionActive = false;
        scheduleNextIdlePersonality(now);
        if (touchStreak == 0 ||
            static_cast<uint32_t>(now - lastTouchAt) >
                TOUCH_REPEAT_WINDOW_MS) {
          touchStreak = 1;
        } else if (touchStreak < 3) {
          touchStreak++;
        }
        lastTouchAt = now;

        if (touchStreak == 1) {
          increaseScore(engagementScore, 20);
        } else if (touchStreak == 2) {
          increaseScore(engagementScore, 25);
        } else {
          increaseScore(engagementScore, 5);
          increaseScore(irritationScore, 25);
        }
        updateMoodState(now);

        const ReactionPlan plan = selectTouchReaction(reactionMood, touchStreak);
        startGenericReaction(now, plan.expression, plan.sound);
      }
      break;
    case BuddyEvent::SoundDetected:
      if (coreState == BuddyCoreState::Awake) {
        // Sleepy may become Calm below; retain Sleepy as reaction context.
        const BuddyMood reactionMood = currentMood;
        lastMeaningfulActivityAt = now;
        increaseScore(engagementScore, 5);
        updateMoodState(now);
        autonomousReactionActive = false;
        scheduleNextIdlePersonality(now);
        if (soundStreak == 0 ||
            static_cast<uint32_t>(now - lastSoundAt) >
                SOUND_REPEAT_WINDOW_MS) {
          soundStreak = 1;
        } else if (soundStreak < 3) {
          soundStreak++;
        }
        lastSoundAt = now;

        const ReactionPlan plan = selectSoundReaction(reactionMood, soundStreak);
        startGenericReaction(now, plan.expression, plan.sound);
      }
      break;
    case BuddyEvent::IdleTimeout:
      if (coreState == BuddyCoreState::Awake &&
          activeReaction == BuddyReaction::Idle && !isSoundEngineActive()) {
        idlePersonalityScheduled = false;
        autonomousReactionActive = true;
        const IdlePersonality personality = selectIdlePersonality();
        startGenericReaction(now, faceExpressionFor(personality),
                             ReactionSound::None);
      }
      break;
  }
}

BuddyCoreState getBuddyCoreState() { return coreState; }
BuddyReaction getBuddyReaction() { return activeReaction; }
BuddyMood getBuddyMood() { return currentMood; }

#if DESK_BUDDY_DIAGNOSTICS
DiagnosticMoodState getDiagnosticMoodState(uint32_t now) {
  return {currentMood, engagementScore, irritationScore,
          coreState == BuddyCoreState::Awake
              ? static_cast<uint32_t>(now - lastMeaningfulActivityAt)
              : 0};
}

void setDiagnosticMood(BuddyMood mood, uint32_t now) {
  lastMoodDecayAt = now;
  engagementScore = mood == BuddyMood::Engaged ? ENGAGED_THRESHOLD : 0;
  irritationScore = mood == BuddyMood::Grumpy ? GRUMPY_THRESHOLD : 0;
  // A forced Sleepy state persists until a real mood-affecting event.
  currentMood = mood;
}

bool triggerDiagnosticReaction(DiagnosticReaction reaction,
                               DiagnosticSoundVariant variant, uint32_t now,
                               uint8_t &selectedVariantIndex) {
  if (coreState != BuddyCoreState::Awake) return false;

  selectedVariantIndex = DIAGNOSTIC_RANDOM_VARIANT;

  autonomousReactionActive = false;
  disableIdlePersonality();

  if (reaction == DiagnosticReaction::Normal) {
    activeReaction = BuddyReaction::Idle;
    stopReactionSound();
    finishFaceReaction(now);
    return true;
  }

  FaceExpression expression = FaceExpression::Normal;
  ReactionSound sound = ReactionSound::None;
  switch (reaction) {
    case DiagnosticReaction::Happy:
      expression = FaceExpression::Happy;
      sound = ReactionSound::Happy;
      break;
    case DiagnosticReaction::Curious:
      expression = FaceExpression::Curious;
      sound = ReactionSound::Curious;
      break;
    case DiagnosticReaction::Annoyed:
      expression = FaceExpression::Annoyed;
      sound = ReactionSound::Annoyed;
      break;
    case DiagnosticReaction::Startled:
      expression = FaceExpression::Startled;
      sound = ReactionSound::Startled;
      break;
    case DiagnosticReaction::Suspicious:
      expression = FaceExpression::Suspicious;
      sound = ReactionSound::Suspicious;
      break;
    case DiagnosticReaction::Confused:
      expression = FaceExpression::Confused;
      sound = ReactionSound::Confused;
      break;
    case DiagnosticReaction::Daydreaming:
      expression = FaceExpression::Daydreaming;
      break;
    case DiagnosticReaction::Normal:
      break;
  }

  uint8_t requestedVariantIndex = DIAGNOSTIC_RANDOM_VARIANT;
  if (variant != DiagnosticSoundVariant::Random) {
    requestedVariantIndex = static_cast<uint8_t>(variant) - 1;
  }
  if (!startDiagnosticReactionSound(now, sound, requestedVariantIndex,
                                    selectedVariantIndex)) {
    return false;
  }

  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now, expression);
  return true;
}
#endif

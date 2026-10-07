#include "BehaviorEngine.h"

#include "FaceRenderer.h"
#include "SoundEngine.h"
#include "SoundSensor.h"

namespace {
constexpr uint32_t TOUCH_REPEAT_WINDOW_MS = 6000;
constexpr uint32_t SOUND_REPEAT_WINDOW_MS = 10000;
constexpr uint32_t CROSS_INTERACTION_WINDOW_MS = 5000;
constexpr uint32_t ENGAGED_AUTONOMOUS_MIN_MS = 12000;
constexpr uint32_t ENGAGED_AUTONOMOUS_MAX_MS = 24000;
constexpr uint32_t GRUMPY_AUTONOMOUS_MIN_MS = 20000;
constexpr uint32_t GRUMPY_AUTONOMOUS_MAX_MS = 35000;
constexpr uint32_t CALM_AUTONOMOUS_MIN_MS = 28000;
constexpr uint32_t CALM_AUTONOMOUS_MAX_MS = 45000;
constexpr uint32_t SLEEPY_AUTONOMOUS_MIN_MS = 45000;
constexpr uint32_t SLEEPY_AUTONOMOUS_MAX_MS = 70000;
constexpr uint8_t MAX_MOOD_SCORE = 100;
constexpr uint8_t ENGAGED_THRESHOLD = 40;
constexpr uint8_t GRUMPY_THRESHOLD = 60;
constexpr uint32_t MOOD_DECAY_INTERVAL_MS = 10000;
constexpr uint32_t SLEEPY_AFTER_INACTIVITY_MS = 90000;

enum class AutonomousBehavior : uint8_t {
  Curious,
  Daydreaming,
  SideGlance,
  Bored,
  SleepyDrift,
  SuspiciousGlance,
  ExcitedScanning,
  AnnoyedSquint
};

struct ReactionPlan {
  FaceExpression expression;
  ReactionSound sound;
};

enum class RecentInteractionType : uint8_t { None, TouchTap, TouchHold, Sound };
struct RecentInteractionContext {
  RecentInteractionType type;
  uint32_t ageMs;
  bool recent;
};

RecentInteractionType lastInteractionType = RecentInteractionType::None;
uint32_t lastInteractionAt = 0;
bool hasLastInteraction = false;

RecentInteractionContext getRecentInteractionContext(uint32_t now) {
  if (!hasLastInteraction) return {RecentInteractionType::None, 0, false};
  const uint32_t age = static_cast<uint32_t>(now - lastInteractionAt);
  return {lastInteractionType, age, age <= CROSS_INTERACTION_WINDOW_MS};
}

void clearRecentInteraction() {
  lastInteractionType = RecentInteractionType::None;
  lastInteractionAt = 0;
  hasLastInteraction = false;
}

void recordRecentInteraction(RecentInteractionType type, uint32_t now) {
  lastInteractionType = type;
  lastInteractionAt = now;
  hasLastInteraction = true;
}

ReactionPlan selectTapReaction(BuddyMood mood, uint8_t streak,
                               RecentInteractionContext previous) {
  if (previous.recent && previous.type == RecentInteractionType::Sound &&
      streak == 1 && (mood == BuddyMood::Calm || mood == BuddyMood::Engaged)) {
    return {FaceExpression::Curious, ReactionSound::Curious};
  }
  if (mood == BuddyMood::Engaged) {
    return streak < 3
        ? ReactionPlan{FaceExpression::Happy, ReactionSound::Happy}
        : ReactionPlan{FaceExpression::Curious, ReactionSound::Curious};
  }
  if (mood == BuddyMood::Grumpy || mood == BuddyMood::Sleepy) {
    return streak == 1
        ? ReactionPlan{FaceExpression::Curious, ReactionSound::Curious}
        : ReactionPlan{FaceExpression::Annoyed, ReactionSound::Annoyed};
  }
  // Calm preserves V1.
  if (streak == 1) {
    return {FaceExpression::Happy, ReactionSound::Happy};
  }
  if (streak == 2) {
    return {FaceExpression::Curious, ReactionSound::Curious};
  }
  return {FaceExpression::Annoyed, ReactionSound::Annoyed};
}

ReactionPlan selectHoldReaction(BuddyMood mood, RecentInteractionContext previous) {
  if (previous.recent && previous.type == RecentInteractionType::Sound) {
    return {FaceExpression::Happy, ReactionSound::Happy};
  }
  if (mood == BuddyMood::Grumpy || mood == BuddyMood::Sleepy) {
    return {FaceExpression::Curious, ReactionSound::Curious};
  }
  return {FaceExpression::Happy, ReactionSound::Happy};
}

ReactionPlan selectSoundReaction(BuddyMood mood, uint8_t streak,
                                 RecentInteractionContext previous) {
  if (previous.recent && previous.type == RecentInteractionType::TouchHold) {
    if (mood == BuddyMood::Engaged) {
      return streak <= 2
          ? ReactionPlan{FaceExpression::Curious, ReactionSound::Curious}
          : ReactionPlan{FaceExpression::Suspicious, ReactionSound::Suspicious};
    }
    if (mood == BuddyMood::Grumpy) {
      if (streak == 1) return {FaceExpression::Curious, ReactionSound::Curious};
      if (streak == 2) return {FaceExpression::Suspicious, ReactionSound::Suspicious};
      return {FaceExpression::Annoyed, ReactionSound::Annoyed};
    }
    if (mood == BuddyMood::Calm && streak == 1) {
      return {FaceExpression::Curious, ReactionSound::Curious};
    }
    if (mood == BuddyMood::Sleepy && streak == 2) {
      return {FaceExpression::Suspicious, ReactionSound::Suspicious};
    }
  }
  if (previous.recent && previous.type == RecentInteractionType::TouchTap &&
      mood == BuddyMood::Calm && streak == 1) {
    return {FaceExpression::Confused, ReactionSound::Confused};
  }
  if (mood == BuddyMood::Grumpy) {
    return streak == 1
        ? ReactionPlan{FaceExpression::Suspicious, ReactionSound::Suspicious}
        : ReactionPlan{FaceExpression::Annoyed, ReactionSound::Annoyed};
  }
  if (mood == BuddyMood::Engaged && streak == 1) {
    return {FaceExpression::Curious, ReactionSound::Curious};
  }
  if (mood == BuddyMood::Sleepy && streak <= 2) {
    return {FaceExpression::Startled, ReactionSound::Startled};
  }
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
BuddySoundMode soundMode = BuddySoundMode::Normal;
uint8_t engagementScore = 0;
uint8_t irritationScore = 0;
uint32_t lastMoodDecayAt = 0;
uint32_t lastMeaningfulActivityAt = 0;
uint32_t lastTouchAt = 0;
uint8_t touchStreak = 0;
uint32_t lastSoundAt = 0;
uint8_t soundStreak = 0;
uint32_t nextAutonomousBehaviorAt = 0;
bool autonomousBehaviorScheduled = false;
BuddyMood autonomousScheduleMood = BuddyMood::Calm;
bool autonomousReactionActive = false;
AutonomousBehavior lastAutonomousBehavior = AutonomousBehavior::Curious;
bool hasLastAutonomousBehavior = false;

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

struct AutonomousTimingRange {
  uint32_t minMs;
  uint32_t maxMs;  // Inclusive; Arduino random() requires maxMs + 1.
};

AutonomousTimingRange autonomousTimingFor(BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Engaged: return {ENGAGED_AUTONOMOUS_MIN_MS, ENGAGED_AUTONOMOUS_MAX_MS};
    case BuddyMood::Grumpy: return {GRUMPY_AUTONOMOUS_MIN_MS, GRUMPY_AUTONOMOUS_MAX_MS};
    case BuddyMood::Sleepy: return {SLEEPY_AUTONOMOUS_MIN_MS, SLEEPY_AUTONOMOUS_MAX_MS};
    case BuddyMood::Calm: return {CALM_AUTONOMOUS_MIN_MS, CALM_AUTONOMOUS_MAX_MS};
  }
  return {CALM_AUTONOMOUS_MIN_MS, CALM_AUTONOMOUS_MAX_MS};
}

void scheduleNextAutonomousBehavior(uint32_t now) {
  autonomousScheduleMood = currentMood;
  const AutonomousTimingRange range = autonomousTimingFor(currentMood);
#if DESK_BUDDY_DIAGNOSTICS
  (void)now;
  (void)range;
  nextAutonomousBehaviorAt = 0;
  autonomousBehaviorScheduled = false;
#else
  nextAutonomousBehaviorAt = now + static_cast<uint32_t>(random(range.minMs, range.maxMs + 1));
  autonomousBehaviorScheduled = true;
#endif
}

void disableAutonomousBehavior() {
  nextAutonomousBehaviorAt = 0;
  autonomousBehaviorScheduled = false;
  autonomousReactionActive = false;
}

struct AutonomousBehaviorPlan {
  FaceExpression expression;
  ReactionSound sound;
};
struct WeightedAutonomousBehavior {
  AutonomousBehavior behavior;
  uint8_t weight;
};
struct AutonomousBehaviorPool {
  const WeightedAutonomousBehavior* candidates;
  uint8_t count;
};
constexpr WeightedAutonomousBehavior CALM_AUTONOMOUS_POOL[] = {
    {AutonomousBehavior::Daydreaming, 30}, {AutonomousBehavior::SideGlance, 25},
    {AutonomousBehavior::Curious, 25}, {AutonomousBehavior::Bored, 20}};
constexpr WeightedAutonomousBehavior ENGAGED_AUTONOMOUS_POOL[] = {
    {AutonomousBehavior::ExcitedScanning, 35}, {AutonomousBehavior::Curious, 30},
    {AutonomousBehavior::SideGlance, 20}, {AutonomousBehavior::Daydreaming, 15}};
constexpr WeightedAutonomousBehavior GRUMPY_AUTONOMOUS_POOL[] = {
    {AutonomousBehavior::AnnoyedSquint, 40}, {AutonomousBehavior::SuspiciousGlance, 35},
    {AutonomousBehavior::SideGlance, 15}, {AutonomousBehavior::Bored, 10}};
constexpr WeightedAutonomousBehavior SLEEPY_AUTONOMOUS_POOL[] = {
    {AutonomousBehavior::SleepyDrift, 45}, {AutonomousBehavior::Daydreaming, 30},
    {AutonomousBehavior::Bored, 20}, {AutonomousBehavior::SideGlance, 5}};

AutonomousBehaviorPool autonomousPoolFor(BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Calm: return {CALM_AUTONOMOUS_POOL, 4};
    case BuddyMood::Engaged: return {ENGAGED_AUTONOMOUS_POOL, 4};
    case BuddyMood::Grumpy: return {GRUMPY_AUTONOMOUS_POOL, 4};
    case BuddyMood::Sleepy: return {SLEEPY_AUTONOMOUS_POOL, 4};
  }
  return {CALM_AUTONOMOUS_POOL, 4};
}

#if defined(DESK_BUDDY_TEST_AUTONOMOUS_SELECTION)
BuddyMood lastAutonomousSelectionMood = BuddyMood::Calm;
#endif

AutonomousBehavior selectAutonomousBehavior(BuddyMood mood) {
#if defined(DESK_BUDDY_TEST_AUTONOMOUS_SELECTION)
  lastAutonomousSelectionMood = mood;
#endif
  const AutonomousBehaviorPool pool = autonomousPoolFor(mood);
  uint16_t totalWeight = 0;
  for (uint8_t index = 0; index < pool.count; ++index) {
    const auto candidate = pool.candidates[index];
    if (!hasLastAutonomousBehavior || candidate.behavior != lastAutonomousBehavior) {
      totalWeight += candidate.weight;
    }
  }
  // A future single-choice pool must still be able to select its sole behavior.
  const bool excludePrevious = totalWeight != 0;
  if (!excludePrevious) {
    for (uint8_t index = 0; index < pool.count; ++index) {
      totalWeight += pool.candidates[index].weight;
    }
  }
  uint16_t ticket = static_cast<uint16_t>(random(totalWeight));
  AutonomousBehavior selected = pool.candidates[0].behavior;
  for (uint8_t index = 0; index < pool.count; ++index) {
    const auto candidate = pool.candidates[index];
    if (excludePrevious && hasLastAutonomousBehavior &&
        candidate.behavior == lastAutonomousBehavior) continue;
    if (ticket < candidate.weight) {
      selected = candidate.behavior;
      break;
    }
    ticket -= candidate.weight;
  }
  lastAutonomousBehavior = selected;
  hasLastAutonomousBehavior = true;
  return selected;
}

AutonomousBehaviorPlan planForAutonomousBehavior(AutonomousBehavior behavior) {
  switch (behavior) {
    case AutonomousBehavior::Curious: return {FaceExpression::Curious, ReactionSound::None};
    case AutonomousBehavior::Daydreaming: return {FaceExpression::Daydreaming, ReactionSound::None};
    case AutonomousBehavior::SideGlance: return {FaceExpression::SideGlance, ReactionSound::None};
    case AutonomousBehavior::Bored: return {FaceExpression::Bored, ReactionSound::None};
    case AutonomousBehavior::SleepyDrift: return {FaceExpression::SleepyDrift, ReactionSound::None};
    case AutonomousBehavior::SuspiciousGlance: return {FaceExpression::SuspiciousGlance, ReactionSound::None};
    case AutonomousBehavior::ExcitedScanning: return {FaceExpression::ExcitedScanning, ReactionSound::None};
    case AutonomousBehavior::AnnoyedSquint: return {FaceExpression::AnnoyedSquint, ReactionSound::None};
  }
  return {FaceExpression::Normal, ReactionSound::None};
}

void resetTouchHistory() {
  lastTouchAt = 0;
  touchStreak = 0;
}

void resetSoundHistory() {
  lastSoundAt = 0;
  soundStreak = 0;
}

void setSoundMode(BuddySoundMode mode) {
  if (soundMode != mode && mode == BuddySoundMode::Quiet) stopReactionSound();
  soundMode = mode;
}

void startGenericReaction(uint32_t now, FaceExpression expression,
                          ReactionSound sound, BuddyMood reactionMood) {
  stopReactionSound();
  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now, expression);
  if (soundMode == BuddySoundMode::Normal) startReactionSound(now, sound, reactionMood);
}

void handleTapInteraction(uint32_t now) {
  if (coreState == BuddyCoreState::Awake) {
    const RecentInteractionContext previous = getRecentInteractionContext(now);
    // Selection uses arrival context, before this touch changes mood.
    const BuddyMood reactionMood = currentMood;
    lastMeaningfulActivityAt = now;
    autonomousReactionActive = false;
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
    scheduleNextAutonomousBehavior(now);

    const ReactionPlan plan = selectTapReaction(reactionMood, touchStreak, previous);
    recordRecentInteraction(RecentInteractionType::TouchTap, now);
    startGenericReaction(now, plan.expression, plan.sound, reactionMood);
  }
}

void handleHoldInteraction(uint32_t now) {
  if (coreState != BuddyCoreState::Awake) return;

  const RecentInteractionContext previous = getRecentInteractionContext(now);
  const BuddyMood reactionMood = currentMood;
  lastMeaningfulActivityAt = now;
  autonomousReactionActive = false;
  increaseScore(engagementScore, 30);
  decreaseScore(irritationScore, 15);
  updateMoodState(now);
  scheduleNextAutonomousBehavior(now);

  const ReactionPlan plan = selectHoldReaction(reactionMood, previous);
  recordRecentInteraction(RecentInteractionType::TouchHold, now);
  startGenericReaction(now, plan.expression, plan.sound, reactionMood);
}

void enterSleep(uint32_t now) {
  clearRecentInteraction();
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = now;
  coreState = BuddyCoreState::Sleeping;
  activeReaction = BuddyReaction::Idle;
  resetTouchHistory();
  resetSoundHistory();
  disableAutonomousBehavior();
  enterSleepFace(now);
  stopReactionSound();
  playSleepSound();
}

void wakeBuddy(uint32_t now) {
  clearRecentInteraction();
  lastMoodDecayAt = now;
  lastMeaningfulActivityAt = now;
  coreState = BuddyCoreState::Awake;
  activeReaction = BuddyReaction::Idle;
  resetSoundHistory();
  autonomousReactionActive = false;
  wakeFace(now);
  playWakeSound();
  ignoreSoundSensorAfterWake();
  updateMoodState(now);
  scheduleNextAutonomousBehavior(now);
}
}  // namespace

void beginBehaviorEngine(uint32_t now) {
  soundMode = BuddySoundMode::Normal;
  clearRecentInteraction();
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
  scheduleNextAutonomousBehavior(now);
}

void updateBehaviorEngine(uint32_t now) {
  bool autonomousCompleted = false;
  const bool scoresDecayed =
      coreState == BuddyCoreState::Awake && decayMoodScores(now);

  if (activeReaction == BuddyReaction::Generic && isFaceReactionFinished(now)) {
    activeReaction = BuddyReaction::Idle;
    finishFaceReaction(now);
    if (autonomousReactionActive) {
      autonomousReactionActive = false;
      autonomousCompleted = true;
    }
  }

  // Evaluate while still Idle, before autonomy can occupy the reaction slot.
  // Before inactivity expires, a forced diagnostic mood lasts until scores
  // change, matching the existing diagnostic selection semantics.
  if (coreState == BuddyCoreState::Awake &&
      (scoresDecayed || static_cast<uint32_t>(now - lastMeaningfulActivityAt) >=
                            SLEEPY_AFTER_INACTIVITY_MS)) {
    updateMoodState(now);
  }

  if (autonomousCompleted) scheduleNextAutonomousBehavior(now);

  // Interaction handlers already stamp their final mood when scheduling.
  // Defer transitions during reactions; autonomous completion owns its schedule.
  if (coreState == BuddyCoreState::Awake &&
      activeReaction == BuddyReaction::Idle && autonomousBehaviorScheduled &&
      !autonomousReactionActive && currentMood != autonomousScheduleMood) {
    scheduleNextAutonomousBehavior(now);
  }

  if (coreState == BuddyCoreState::Awake && autonomousBehaviorScheduled &&
      timeReached(now, nextAutonomousBehaviorAt)) {
    if (activeReaction == BuddyReaction::Idle && !isSoundEngineActive()) {
      autonomousBehaviorScheduled = false;
      processBuddyEvent(BuddyEvent::IdleTimeout, now);
    } else {
      scheduleNextAutonomousBehavior(now);
    }
  }
}

void processBuddyEvent(BuddyEvent event, uint32_t now) {
  switch (event) {
    case BuddyEvent::ButtonShortPress:
      if (coreState == BuddyCoreState::Sleeping) {
        wakeBuddy(now);
      } else {
        enterSleep(now);
      }
      break;
    case BuddyEvent::ButtonLongPress:
      if (coreState == BuddyCoreState::Sleeping) {
        wakeBuddy(now);
      } else {
        setSoundMode(soundMode == BuddySoundMode::Normal
            ? BuddySoundMode::Quiet : BuddySoundMode::Normal);
      }
      break;
    case BuddyEvent::TouchTap:
      handleTapInteraction(now);
      break;
    case BuddyEvent::TouchHold:
      handleHoldInteraction(now);
      break;
    case BuddyEvent::SoundDetected:
      if (coreState == BuddyCoreState::Awake) {
        // Sleepy may become Calm below; retain Sleepy as reaction context.
        const BuddyMood reactionMood = currentMood;
        const RecentInteractionContext previous = getRecentInteractionContext(now);
        lastMeaningfulActivityAt = now;
        increaseScore(engagementScore, 5);
        updateMoodState(now);
        autonomousReactionActive = false;
        scheduleNextAutonomousBehavior(now);
        if (soundStreak == 0 ||
            static_cast<uint32_t>(now - lastSoundAt) >
                SOUND_REPEAT_WINDOW_MS) {
          soundStreak = 1;
        } else if (soundStreak < 3) {
          soundStreak++;
        }
        lastSoundAt = now;

        const ReactionPlan plan = selectSoundReaction(reactionMood, soundStreak, previous);
        recordRecentInteraction(RecentInteractionType::Sound, now);
        startGenericReaction(now, plan.expression, plan.sound, reactionMood);
      }
      break;
    case BuddyEvent::IdleTimeout:
      if (coreState == BuddyCoreState::Awake &&
          activeReaction == BuddyReaction::Idle && !isSoundEngineActive()) {
        // Explicit IdleTimeout callers also need decay and genuine idle mood.
        decayMoodScores(now);
        updateMoodState(now);
        autonomousBehaviorScheduled = false;
        autonomousReactionActive = true;
        const AutonomousBehavior behavior = selectAutonomousBehavior(currentMood);
        const AutonomousBehaviorPlan plan = planForAutonomousBehavior(behavior);
        startGenericReaction(now, plan.expression, plan.sound, currentMood);
      }
      break;
  }
}

#if defined(DESK_BUDDY_TEST_AUTONOMOUS_SELECTION)
BuddyMood getTestAutonomousSelectionMood() { return lastAutonomousSelectionMood; }
#endif

BuddyCoreState getBuddyCoreState() { return coreState; }
BuddyReaction getBuddyReaction() { return activeReaction; }
BuddyMood getBuddyMood() { return currentMood; }

#if DESK_BUDDY_DIAGNOSTICS
DiagnosticAutonomousTimingState getDiagnosticAutonomousTimingState(uint32_t now) {
  const BuddyMood mood = autonomousBehaviorScheduled ? autonomousScheduleMood : currentMood;
  const AutonomousTimingRange range = autonomousTimingFor(mood);
  const uint32_t remaining = autonomousBehaviorScheduled &&
      !timeReached(now, nextAutonomousBehaviorAt)
      ? static_cast<uint32_t>(nextAutonomousBehaviorAt - now) : 0;
  return {autonomousBehaviorScheduled, mood, remaining, range.minMs, range.maxMs};
}

bool triggerDiagnosticAutonomousBehavior(DiagnosticAutonomousBehavior requested,
                                         uint32_t now) {
  if (coreState != BuddyCoreState::Awake) return false;
  AutonomousBehavior behavior;
  switch (requested) {
    case DiagnosticAutonomousBehavior::Curious: behavior = AutonomousBehavior::Curious; break;
    case DiagnosticAutonomousBehavior::Daydreaming: behavior = AutonomousBehavior::Daydreaming; break;
    case DiagnosticAutonomousBehavior::SideGlance: behavior = AutonomousBehavior::SideGlance; break;
    case DiagnosticAutonomousBehavior::Bored: behavior = AutonomousBehavior::Bored; break;
    case DiagnosticAutonomousBehavior::SuspiciousGlance: behavior = AutonomousBehavior::SuspiciousGlance; break;
    case DiagnosticAutonomousBehavior::AnnoyedSquint: behavior = AutonomousBehavior::AnnoyedSquint; break;
    case DiagnosticAutonomousBehavior::SleepyDrift: behavior = AutonomousBehavior::SleepyDrift; break;
    case DiagnosticAutonomousBehavior::ExcitedScanning: behavior = AutonomousBehavior::ExcitedScanning; break;
    default: return false;
  }
  // Showcase does not update selection history or meaningful interaction state.
  autonomousReactionActive = false;
  disableAutonomousBehavior();
  const AutonomousBehaviorPlan plan = planForAutonomousBehavior(behavior);
  startGenericReaction(now, plan.expression, plan.sound, currentMood);
  return true;
}

DiagnosticRecentInteractionContext getDiagnosticRecentInteractionContext(uint32_t now) {
  const RecentInteractionContext context = getRecentInteractionContext(now);
  DiagnosticRecentInteractionType type = DiagnosticRecentInteractionType::None;
  switch (context.type) {
    case RecentInteractionType::None: break;
    case RecentInteractionType::TouchTap:
      type = DiagnosticRecentInteractionType::TouchTap;
      break;
    case RecentInteractionType::TouchHold:
      type = DiagnosticRecentInteractionType::TouchHold;
      break;
    case RecentInteractionType::Sound:
      type = DiagnosticRecentInteractionType::Sound;
      break;
  }
  return {type, context.ageMs, context.recent};
}

DiagnosticMoodState getDiagnosticMoodState(uint32_t now) {
  return {currentMood, engagementScore, irritationScore,
          coreState == BuddyCoreState::Awake
              ? static_cast<uint32_t>(now - lastMeaningfulActivityAt)
              : 0};
}

void setDiagnosticSoundMode(BuddySoundMode mode) { setSoundMode(mode); }

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
  disableAutonomousBehavior();

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
                                    selectedVariantIndex, currentMood)) {
    return false;
  }

  activeReaction = BuddyReaction::Generic;
  startFaceReaction(now, expression);
  return true;
}
#endif

BuddySoundMode getBuddySoundMode() { return soundMode; }

#include <cassert>
#include <cstdio>
#include "BuddyLifeSimulator.h"

namespace {
void scores(const BuddyLifeSnapshot& state, BuddyMood mood,
            uint8_t engagement, uint8_t irritation) {
  assert(state.mood == mood);
  assert(state.engagement == engagement && state.irritation == irritation);
}
void reaction(const BuddyLifeSnapshot& state, FaceExpression expression,
              ReactionSound sound, BuddyMood arrivalMood) {
  assert(state.reaction == BuddyReaction::Generic);
  assert(state.lastExpression == expression && state.lastSound == sound);
  assert(state.lastSoundMood == arrivalMood);
}
void fiveRapidTaps(BuddyLifeSimulator& buddy) {
  // t=0 fresh; taps at 100, 200, 300, 400, 500 ms stay inside one streak.
  buddy.reset();
  for (uint32_t at = 100; at <= 500; at += 100) {
    buddy.advanceTo(at);
    buddy.tap();
  }
}

void friendlyAttention() {
  // t=0 Calm; t=100 Tap -> Calm; t=500 Tap -> Engaged (arrival still Calm).
  BuddyLifeSimulator buddy;
  buddy.reset();
  scores(buddy.snapshot(), BuddyMood::Calm, 0, 0);
  buddy.advanceTo(100); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Calm, 20, 0);
  reaction(buddy.snapshot(), FaceExpression::Happy, ReactionSound::Happy, BuddyMood::Calm);
  buddy.advanceTo(500); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  reaction(buddy.snapshot(), FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Calm);
}

void excessiveInteraction() {
  // t=100/200 friendly taps; t=300/400/500 irritation accumulates -> Grumpy.
  BuddyLifeSimulator buddy;
  buddy.reset();
  buddy.advanceTo(100); buddy.tap();
  buddy.advanceTo(200); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  buddy.advanceTo(300); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Engaged, 50, 25);
  buddy.advanceTo(400); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Engaged, 55, 50);
  buddy.advanceTo(500); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Grumpy, 60, 75);
  // Crossing Grumpy still selects using pre-event Engaged.
  reaction(buddy.snapshot(), FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Engaged);
}

void gradualRecovery() {
  // Last activity t=500; decay anchored at reset t=0, not the final Tap.
  // t=10s Grumpy; t=20s Engaged; t=50s Calm. No activity during recovery.
  BuddyLifeSimulator buddy;
  fiveRapidTaps(buddy);
  buddy.advanceTo(9999); scores(buddy.snapshot(), BuddyMood::Grumpy, 60, 75);
  buddy.advanceTo(10000); scores(buddy.snapshot(), BuddyMood::Grumpy, 55, 65);
  buddy.advanceTo(19999); scores(buddy.snapshot(), BuddyMood::Grumpy, 55, 65);
  buddy.advanceTo(20000); scores(buddy.snapshot(), BuddyMood::Engaged, 50, 55);
  buddy.advanceTo(40000); scores(buddy.snapshot(), BuddyMood::Engaged, 40, 35);
  buddy.advanceTo(49999); scores(buddy.snapshot(), BuddyMood::Engaged, 40, 35);
  buddy.advanceTo(50000); scores(buddy.snapshot(), BuddyMood::Calm, 35, 25);
  assert(buddy.snapshot().inactivityMs == 49500);
}

void recoveredCalmBecomesSleepy() {
  // t=500 final Tap; t=50s Calm; t=90500 exactly 90s inactive -> Sleepy.
  BuddyLifeSimulator buddy;
  fiveRapidTaps(buddy);
  buddy.advanceTo(50000); scores(buddy.snapshot(), BuddyMood::Calm, 35, 25);
  buddy.advanceTo(90499); scores(buddy.snapshot(), BuddyMood::Calm, 15, 0);
  assert(buddy.snapshot().inactivityMs == 89999);
  buddy.advanceTo(90500); scores(buddy.snapshot(), BuddyMood::Sleepy, 15, 0);
  assert(buddy.snapshot().inactivityMs == 90000);
}

void sleepySoundArrival() {
  // t=90s naturally Sleepy and Idle; Sound uses Sleepy context then restores Calm.
  BuddyLifeSimulator buddy;
  buddy.reset(); buddy.advanceTo(90000);
  scores(buddy.snapshot(), BuddyMood::Sleepy, 0, 0);
  assert(buddy.snapshot().reaction == BuddyReaction::Idle);
  buddy.sound();
  reaction(buddy.snapshot(), FaceExpression::Startled, ReactionSound::Startled, BuddyMood::Sleepy);
  scores(buddy.snapshot(), BuddyMood::Calm, 5, 0);
  assert(buddy.snapshot().inactivityMs == 0 && buddy.snapshot().soundActive);
  assert(buddy.snapshot().recentInteraction == DiagnosticRecentInteractionType::Sound);
  buddy.advanceBy(1800);
  assert(buddy.snapshot().reaction == BuddyReaction::Idle);
  assert(buddy.snapshot().inactivityMs == 1800);
}

void affectionRecovery() {
  // t=500 Grumpy 60/75; t=2300 Hold -> 90/60 still Grumpy;
  // t=4100 distinct Hold -> 100/45 Engaged (both arrive Grumpy).
  BuddyLifeSimulator buddy;
  fiveRapidTaps(buddy);
  buddy.advanceTo(2300); assert(buddy.snapshot().reaction == BuddyReaction::Idle);
  buddy.hold(); scores(buddy.snapshot(), BuddyMood::Grumpy, 90, 60);
  reaction(buddy.snapshot(), FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Grumpy);
  buddy.advanceTo(4100); assert(buddy.snapshot().reaction == BuddyReaction::Idle);
  buddy.hold(); scores(buddy.snapshot(), BuddyMood::Engaged, 100, 45);
  reaction(buddy.snapshot(), FaceExpression::Curious, ReactionSound::Curious, BuddyMood::Grumpy);
  assert(buddy.snapshot().recentInteraction == DiagnosticRecentInteractionType::TouchHold);
}

void autonomousInactivity() {
  // t=30/55/80s explicit autonomous events; each finishes at +1800ms.
  // No meaningful activity: t=90s must still become Sleepy.
  BuddyLifeSimulator buddy;
  buddy.reset();
  for (uint32_t at : {30000U, 55000U, 80000U}) {
    buddy.advanceTo(at);
    assert(buddy.snapshot().reaction == BuddyReaction::Idle);
    buddy.queueRandom(0); buddy.autonomousEvent();
    const auto started = buddy.snapshot();
    assert(started.reaction == BuddyReaction::Generic);
    assert(started.lastExpression != FaceExpression::Normal);
    assert(started.lastSound == ReactionSound::None && !started.soundActive);
    assert(started.inactivityMs == at && !started.recentInteractionValid);
    assert(started.recentInteraction == DiagnosticRecentInteractionType::None);
    buddy.advanceBy(1800);
    assert(buddy.snapshot().reaction == BuddyReaction::Idle);
    assert(buddy.snapshot().inactivityMs == at + 1800);
  }
  buddy.advanceTo(89999); scores(buddy.snapshot(), BuddyMood::Calm, 0, 0);
  buddy.advanceTo(90000); scores(buddy.snapshot(), BuddyMood::Sleepy, 0, 0);
  assert(buddy.snapshot().inactivityMs == 90000);
}

void physicalSleepPause() {
  // t=100/500 taps -> 45/0 Engaged; t=501 sleep for 180000ms.
  // Wake t=180501 resets decay anchor; first awake decay at t=190501.
  BuddyLifeSimulator buddy;
  buddy.reset(); buddy.advanceTo(100); buddy.tap();
  buddy.advanceTo(500); buddy.tap();
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  buddy.advanceTo(501); buddy.shortButtonPress();
  assert(buddy.snapshot().coreState == BuddyCoreState::Sleeping);
  buddy.advanceBy(180000);
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  assert(buddy.snapshot().inactivityMs == 0);
  buddy.shortButtonPress();
  assert(buddy.snapshot().coreState == BuddyCoreState::Awake);
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  assert(buddy.snapshot().inactivityMs == 0);
  buddy.advanceBy(9999); scores(buddy.snapshot(), BuddyMood::Engaged, 45, 0);
  buddy.advanceBy(1); scores(buddy.snapshot(), BuddyMood::Engaged, 40, 0);
  buddy.advanceBy(10000); scores(buddy.snapshot(), BuddyMood::Calm, 35, 0);
}

void crossInteractionExpiry() {
  // t=100 Tap; t=2100 first Sound -> Confused (Tap age 2000ms).
  // t=12101: both Tap context (>5s) and Sound streak (>10s) expired.
  BuddyLifeSimulator buddy;
  buddy.reset(); buddy.advanceTo(100); buddy.tap();
  buddy.advanceTo(2100);
  assert(buddy.snapshot().recentInteractionValid && buddy.snapshot().recentInteractionAgeMs == 2000);
  buddy.sound();
  reaction(buddy.snapshot(), FaceExpression::Confused, ReactionSound::Confused, BuddyMood::Calm);
  buddy.advanceTo(12101);
  const auto expired = buddy.snapshot();
  assert(expired.reaction == BuddyReaction::Idle && !expired.recentInteractionValid);
  assert(expired.recentInteractionAgeMs == 10001);
  assert(expired.recentInteraction == DiagnosticRecentInteractionType::Sound);
  buddy.sound();
  reaction(buddy.snapshot(), FaceExpression::Startled, ReactionSound::Startled, BuddyMood::Calm);
}

void personalityParity(const BuddyLifeSnapshot& normal, const BuddyLifeSnapshot& quiet) {
  assert(normal.now == quiet.now && normal.coreState == quiet.coreState);
  assert(normal.reaction == quiet.reaction && normal.mood == quiet.mood);
  assert(normal.engagement == quiet.engagement && normal.irritation == quiet.irritation);
  assert(normal.lastExpression == quiet.lastExpression && normal.inactivityMs == quiet.inactivityMs);
  assert(normal.recentInteraction == quiet.recentInteraction);
  assert(normal.recentInteractionAgeMs == quiet.recentInteractionAgeMs);
  assert(normal.recentInteractionValid == quiet.recentInteractionValid);
  assert(normal.soundMode == BuddySoundMode::Normal && quiet.soundMode == BuddySoundMode::Quiet);
}
void quietParity() {
  // Both runs: t=100/500/900 taps; t=10000 decay. Quiet toggled at t=0 only.
  BuddyLifeSnapshot normal[4];
  BuddyLifeSimulator buddy;
  buddy.reset();
  size_t index = 0;
  for (uint32_t at : {100U, 500U, 900U}) {
    buddy.advanceTo(at); buddy.tap(); normal[index++] = buddy.snapshot();
    assert(normal[index - 1].soundActive);
  }
  buddy.advanceTo(10000); normal[3] = buddy.snapshot();
  buddy.reset(); buddy.longButtonPress();
  index = 0;
  for (uint32_t at : {100U, 500U, 900U}) {
    buddy.advanceTo(at); buddy.tap();
    const auto quiet = buddy.snapshot();
    personalityParity(normal[index++], quiet);
    assert(!quiet.soundActive && quiet.lastSound == ReactionSound::None);
  }
  buddy.advanceTo(10000); personalityParity(normal[3], buddy.snapshot());
  scores(buddy.snapshot(), BuddyMood::Engaged, 45, 15);
}
}

int main() {
  const struct { const char* name; void (*run)(); } scenarios[] = {
    {"Friendly attention -> Engaged", friendlyAttention},
    {"Excessive interaction -> Grumpy", excessiveInteraction},
    {"Grumpy -> Engaged -> Calm recovery", gradualRecovery},
    {"Recovered Calm inactivity -> Sleepy", recoveredCalmBecomesSleepy},
    {"Sleepy Sound arrival -> Startled/Calm", sleepySoundArrival},
    {"Affection recovers Grumpy", affectionRecovery},
    {"Autonomous activity preserves inactivity", autonomousInactivity},
    {"Physical sleep pauses decay", physicalSleepPause},
    {"Cross-interaction expiry", crossInteractionExpiry},
    {"Quiet personality parity", quietParity},
  };
  for (const auto& scenario : scenarios) {
    std::printf("[SCENARIO] %s\n", scenario.name);
    std::fflush(stdout);
    scenario.run();
  }
}

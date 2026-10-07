#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "BuddyLifeSimulator.h"

namespace {
using E = SimulatedBuddyEvent;
// Ten minutes, all moods reached naturally; checkpoints inject no production event.
constexpr ScheduledBuddyEvent life[] = {
  // Friendly attention -> excess -> decay through Engaged to Calm.
  {5000,E::Tap}, {6000,E::Tap}, {7000,E::Tap}, {8000,E::Tap}, {9000,E::Tap},
  {10000,E::Checkpoint}, {20000,E::Checkpoint}, {30000,E::Autonomous},
  {31800,E::Checkpoint}, {50000,E::Checkpoint}, {60000,E::Autonomous},
  {61800,E::Checkpoint}, {90000,E::Autonomous}, {91800,E::Checkpoint},
  {99000,E::Checkpoint}, {100000,E::Sound},
  // Tap->Sound, expiry, Sound->Hold, Hold->Sound, Sound->fresh Tap.
  {110000,E::Tap}, {112000,E::Sound}, {123001,E::Sound}, {124000,E::Hold},
  {125000,E::Sound}, {132000,E::Sound}, {133000,E::Tap},
  // Quiet personality, gradual Hold recovery, then 60 seconds physical sleep.
  {140000,E::LongButtonPress}, {141000,E::Tap}, {142000,E::Tap},
  {143000,E::Tap}, {144000,E::Tap}, {145000,E::Tap},
  {147000,E::Hold}, {149000,E::Hold}, {150000,E::ShortButtonPress},
  {210000,E::ShortButtonPress}, {211000,E::LongButtonPress},
  // Awake decay resumes; score precedence can delay Sleepy beyond 90 seconds.
  {220000,E::Checkpoint}, {240000,E::Autonomous}, {241800,E::Checkpoint},
  {270000,E::Autonomous}, {271800,E::Checkpoint}, {299999,E::Checkpoint},
  {300000,E::Checkpoint}, {330000,E::Autonomous}, {331800,E::Checkpoint},
  {600000,E::Checkpoint}
};
constexpr ScheduledBuddyEvent rollover[] = {
  {5000,E::Tap}, {6000,E::Tap}, {7000,E::Tap}, {8000,E::Tap}, {9000,E::Tap},
  {50000,E::Checkpoint}, {99000,E::Checkpoint}, {100000,E::Sound},
  // Wrap is at elapsed 120001 ms. Replacements, decay, and memory span wrap.
  {119000,E::Hold}, {120000,E::Sound}, {121000,E::Tap},
  {122799,E::Checkpoint}, {122800,E::Checkpoint},
  {126001,E::Checkpoint}, {210999,E::Checkpoint}, {211000,E::Checkpoint}
};
BuddyLifeSnapshot current{};
const char* currentEvent = "setup";
const char* currentRun = "script validation";
uint32_t currentStart = 0;
[[noreturn]] void fail(const char* condition, int line) {
  std::fprintf(stderr,
    "%s failed at line %d: %s\nt=%u ms elapsed=%u event=%s core=%u reaction=%u "
    "mood=%u engagement=%u irritation=%u mode=%u inactivity=%u "
    "memory=%u age=%u recent=%d face=%u sound=%u soundMood=%u audio=%d\n",
    currentRun, line, condition, current.now, static_cast<uint32_t>(current.now-currentStart),
    currentEvent, unsigned(current.coreState), unsigned(current.reaction), unsigned(current.mood),
    current.engagement, current.irritation, unsigned(current.soundMode), current.inactivityMs,
    unsigned(current.recentInteraction), current.recentInteractionAgeMs, current.recentInteractionValid,
    unsigned(current.lastExpression), unsigned(current.lastSound), unsigned(current.lastSoundMood),
    current.soundActive);
  std::abort();
}
#define CHECK(value) do { if (!(value)) fail(#value, __LINE__); } while (false)

// Applicable to naturally evolved production state, without diagnostic mood forcing.
void assertBuddyStateInvariants(const BuddyLifeSnapshot& s) {
  CHECK(s.engagement <= 100 && s.irritation <= 100);
  CHECK(s.mood == BuddyMood::Calm || s.mood == BuddyMood::Engaged ||
        s.mood == BuddyMood::Grumpy || s.mood == BuddyMood::Sleepy);
  CHECK(s.coreState == BuddyCoreState::Awake || s.coreState == BuddyCoreState::Sleeping);
  CHECK(s.reaction == BuddyReaction::Idle || s.reaction == BuddyReaction::Generic);
  CHECK(s.soundMode == BuddySoundMode::Normal || s.soundMode == BuddySoundMode::Quiet);
  CHECK(s.recentInteraction == DiagnosticRecentInteractionType::None ||
        s.recentInteraction == DiagnosticRecentInteractionType::TouchTap ||
        s.recentInteraction == DiagnosticRecentInteractionType::TouchHold ||
        s.recentInteraction == DiagnosticRecentInteractionType::Sound);
  if (s.recentInteraction == DiagnosticRecentInteractionType::None) {
    CHECK(!s.recentInteractionValid && s.recentInteractionAgeMs == 0);
  } else {
    CHECK(s.recentInteractionValid == (s.recentInteractionAgeMs <= 5000));
  }
  if (s.coreState == BuddyCoreState::Sleeping) {
    CHECK(s.inactivityMs == 0 && s.reaction == BuddyReaction::Idle);
  } else if (s.irritation >= 60) {
    CHECK(s.mood == BuddyMood::Grumpy);
  } else if (s.engagement >= 40) {
    CHECK(s.mood == BuddyMood::Engaged);
  }
  // Quiet may still play boot/sleep/wake system cues: do not forbid all audio.
}
void scores(BuddyMood mood, uint8_t engagement, uint8_t irritation) {
  CHECK(current.mood == mood);
  CHECK(current.engagement == engagement && current.irritation == irritation);
}
void expression(FaceExpression face) {
  CHECK(current.reaction == BuddyReaction::Generic && current.lastExpression == face);
}
void samePersonality(const BuddyLifeSnapshot& a, const BuddyLifeSnapshot& b) {
  CHECK(a.now == b.now && a.coreState == b.coreState && a.reaction == b.reaction);
  CHECK(a.mood == b.mood && a.engagement == b.engagement && a.irritation == b.irritation);
  CHECK(a.inactivityMs == b.inactivityMs && a.lastExpression == b.lastExpression);
  CHECK(a.recentInteraction == b.recentInteraction);
  CHECK(a.recentInteractionAgeMs == b.recentInteractionAgeMs);
  CHECK(a.recentInteractionValid == b.recentInteractionValid);
}
// Fixed-capacity baseline: no heap, sorting, or probabilistic comparisons.
std::array<BuddyLifeSnapshot, 6500> normalTrace{};
size_t normalCount = 0;
struct RunContext {
  uint32_t start;
  bool quietRun;
  bool rolloverRun;
  size_t observations = 0;
  size_t ticks = 0;
  size_t autonomousCount = 0;
  size_t autonomousFinishes = 0;
  BuddyLifeSnapshot before{};
  bool awaitingAutonomousFinish = false;
  uint32_t autonomousStarted = 0;
  uint32_t milestones = 0;
};
// One bit for each required named milestone, checked at the end of each story.
constexpr uint32_t ENGAGED = 1U << 0;
constexpr uint32_t GRUMPY = 1U << 1;
constexpr uint32_t CALM_RECOVERY = 1U << 2;
constexpr uint32_t SLEEPY = 1U << 3;
constexpr uint32_t SLEEPY_SOUND = 1U << 4;
constexpr uint32_t HOLD_RECOVERY = 1U << 5;
constexpr uint32_t SLEEP_WAKE = 1U << 6;
constexpr uint32_t QUIET_PARITY = 1U << 7;
constexpr uint32_t ALL_MILESTONES = 255;

void milestones(RunContext& run, uint32_t elapsed) {
  if (elapsed == 6000) { scores(BuddyMood::Engaged,45,0); run.milestones |= ENGAGED; }
  if (elapsed == 9000) { scores(BuddyMood::Grumpy,60,75); run.milestones |= GRUMPY; }
  if (elapsed == 50000) { scores(BuddyMood::Calm,35,25); run.milestones |= CALM_RECOVERY; }
  if (elapsed == 99000) {
    scores(BuddyMood::Sleepy,15,0);
    CHECK(current.inactivityMs == 90000 && current.reaction == BuddyReaction::Idle);
    run.milestones |= SLEEPY;
  }
  if (elapsed == 100000) {
    scores(BuddyMood::Calm,15,0); expression(FaceExpression::Startled);
    CHECK(current.lastSound == ReactionSound::Startled && current.lastSoundMood == BuddyMood::Sleepy);
    CHECK(current.inactivityMs == 0);
    run.milestones |= SLEEPY_SOUND;
  }
  if (run.rolloverRun) {
    if (elapsed == 119000) { scores(BuddyMood::Engaged,40,0); expression(FaceExpression::Happy); }
    if (elapsed == 120000) {
      scores(BuddyMood::Engaged,40,0); expression(FaceExpression::Curious);
      CHECK(run.before.mood == BuddyMood::Calm && run.before.engagement == 35);
      CHECK(current.lastSoundMood == BuddyMood::Calm);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::TouchHold);
    }
    if (elapsed == 121000) {
      scores(BuddyMood::Engaged,60,0); expression(FaceExpression::Curious);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::Sound);
      CHECK(run.before.recentInteractionAgeMs == 1000 && run.before.recentInteractionValid);
    }
    if (elapsed == 122799) CHECK(current.reaction == BuddyReaction::Generic);
    if (elapsed == 122800) CHECK(current.reaction == BuddyReaction::Idle);
    if (elapsed == 126001) {
      CHECK(current.recentInteractionAgeMs == 5001 && !current.recentInteractionValid);
      CHECK(current.inactivityMs == 5001);
    }
    if (elapsed == 210999) { scores(BuddyMood::Calm,15,0); CHECK(current.inactivityMs == 89999); }
    if (elapsed == 211000) { scores(BuddyMood::Sleepy,15,0); CHECK(current.inactivityMs == 90000); }
    return;
  }
  switch (elapsed) {
    case 10000: scores(BuddyMood::Grumpy,55,65); break;
    case 20000: scores(BuddyMood::Engaged,50,55); break;
    case 112000:
      expression(FaceExpression::Confused);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::TouchTap);
      CHECK(run.before.recentInteractionAgeMs == 2000 && run.before.recentInteractionValid);
      break;
    case 123001:
      expression(FaceExpression::Startled);
      CHECK(run.before.mood == BuddyMood::Calm && !run.before.recentInteractionValid);
      CHECK(run.before.recentInteractionAgeMs == 11001); // Also expires independent Sound streak.
      break;
    case 124000:
      expression(FaceExpression::Happy);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::Sound);
      CHECK(run.before.recentInteractionValid); break;
    case 125000:
      expression(FaceExpression::Curious);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::TouchHold);
      CHECK(run.before.recentInteractionValid); break;
    case 133000:
      expression(FaceExpression::Curious);
      CHECK(run.before.recentInteraction == DiagnosticRecentInteractionType::Sound);
      CHECK(run.before.recentInteractionAgeMs == 1000 && run.before.recentInteractionValid); break;
    case 145000: scores(BuddyMood::Grumpy,100,75); break;
    case 147000:
      scores(BuddyMood::Grumpy,100,60); expression(FaceExpression::Curious); break;
    case 149000:
      scores(BuddyMood::Engaged,100,45); expression(FaceExpression::Curious);
      run.milestones |= HOLD_RECOVERY; break;
    case 150000:
      scores(BuddyMood::Engaged,95,35);
      CHECK(current.coreState == BuddyCoreState::Sleeping && current.soundActive); break;
    case 210000:
      scores(BuddyMood::Engaged,95,35);
      CHECK(current.coreState == BuddyCoreState::Awake && current.inactivityMs == 0);
      CHECK(current.soundActive); // Wake cue still plays in Quiet.
      run.milestones |= SLEEP_WAKE; break;
    case 220000: scores(BuddyMood::Engaged,90,25); break;
    case 299999: scores(BuddyMood::Engaged,55,0); CHECK(current.inactivityMs == 89999); break;
    case 300000: scores(BuddyMood::Engaged,50,0); CHECK(current.inactivityMs == 90000); break;
    case 600000:
      scores(BuddyMood::Sleepy,0,0);
      CHECK(current.inactivityMs == 390000 && current.reaction == BuddyReaction::Idle); break;
    default: break;
  }
}

void observe(const BuddyLifeSnapshot& s, const char* event, void* context) {
  auto& run = *static_cast<RunContext*>(context);
  current = s; currentEvent = event; currentStart = run.start;
  const uint32_t elapsed = static_cast<uint32_t>(s.now - run.start);
  assertBuddyStateInvariants(s);
  if (!run.rolloverRun) {
    if (!run.quietRun) {
      CHECK(run.observations < normalTrace.size());
      normalTrace[run.observations] = s;
    } else {
      CHECK(run.observations < normalCount);
      samePersonality(normalTrace[run.observations],s);
      const bool quietExpected = (elapsed > 140000 && elapsed < 211000) ||
          (elapsed == 140000 && std::strcmp(event,"LongButtonPress") == 0) ||
          (elapsed == 211000 && std::strcmp(event,"Tick") == 0);
      CHECK(s.soundMode == (quietExpected ? BuddySoundMode::Quiet : BuddySoundMode::Normal));
      run.milestones |= QUIET_PARITY;
    }
  }
  ++run.observations;
  if (std::strcmp(event,"Tick") == 0) {
    ++run.ticks;
    if (run.awaitingAutonomousFinish &&
        static_cast<uint32_t>(s.now-run.autonomousStarted) >= 1800) {
      CHECK(s.reaction == BuddyReaction::Idle);
      run.awaitingAutonomousFinish = false;
      ++run.autonomousFinishes;
    }
    run.before = s;
    return;
  }
  if (std::strcmp(event,"Autonomous") == 0) {
    CHECK(run.before.reaction == BuddyReaction::Idle && s.reaction == BuddyReaction::Generic);
    CHECK(s.lastExpression != FaceExpression::Normal && !s.soundActive);
    CHECK(s.engagement == run.before.engagement && s.irritation == run.before.irritation);
    CHECK(s.inactivityMs == run.before.inactivityMs);
    CHECK(s.recentInteraction == run.before.recentInteraction);
    CHECK(s.recentInteractionAgeMs == run.before.recentInteractionAgeMs);
    CHECK(s.recentInteractionValid == run.before.recentInteractionValid);
    run.awaitingAutonomousFinish = true; run.autonomousStarted = s.now;
    ++run.autonomousCount;
  }
  const bool interaction = std::strcmp(event,"Tap") == 0 || std::strcmp(event,"Hold") == 0 ||
                           std::strcmp(event,"Sound") == 0;
  if (interaction && s.coreState == BuddyCoreState::Awake) {
    CHECK(s.reaction == BuddyReaction::Generic && s.lastExpression != FaceExpression::Normal);
    CHECK(s.soundActive == (s.soundMode == BuddySoundMode::Normal));
  }
  milestones(run,elapsed);
}

void queueRandomStream(BuddyLifeSimulator& buddy) {
  // Explicit selector tickets. Sound is a fixed activity stub, not a variant model.
  // Queue exhaustion uses the harness's documented zero/minimum fallback.
  for (uint32_t ticket : {0U,10U,24U,30U,60U,7U}) buddy.queueRandom(ticket);
}
void runLife(bool quiet) {
  currentRun = quiet ? "ten-minute Quiet timeline" : "ten-minute Normal control";
  BuddyLifeSimulator buddy;
  RunContext context{0,quiet,false};
  buddy.setObserver(observe,&context); buddy.reset(); queueRandomStream(buddy);
  std::array<ScheduledBuddyEvent, sizeof(life)/sizeof(life[0])> script{};
  for (size_t index = 0; index < script.size(); ++index) {
    script[index] = life[index];
    // Same timestamps/observations; only mode toggles become checkpoints in control.
    if (!quiet && script[index].event == E::LongButtonPress) script[index].event = E::Checkpoint;
  }
  CHECK(buddy.runScript(script.data(),script.size(),600000));
  CHECK(static_cast<uint32_t>(buddy.now()-context.start) == 600000);
  CHECK(context.ticks >= 6000 && context.ticks < 6200);
  CHECK(context.autonomousCount == 6 && context.autonomousFinishes == 6);
  CHECK(!context.awaitingAutonomousFinish);
  if (!quiet) {
    CHECK(context.milestones == (ALL_MILESTONES & ~QUIET_PARITY));
    normalCount = context.observations;
  } else {
    CHECK(context.milestones == ALL_MILESTONES);
    CHECK(context.observations == normalCount);
  }
  std::printf("[PASS] %s: 600000 virtual ms, %zu invariant observations\n",currentRun,context.observations);
}
void runRollover() {
  currentRun = "multi-minute rollover timeline";
  BuddyLifeSimulator buddy;
  constexpr uint32_t start = UINT32_MAX-120000;
  RunContext context{start,false,true};
  buddy.setObserver(observe,&context); buddy.reset(start); queueRandomStream(buddy);
  CHECK(buddy.runScript(rollover,sizeof(rollover)/sizeof(rollover[0]),211000));
  CHECK(static_cast<uint32_t>(buddy.now()-start) == 211000 && buddy.now() < start);
  CHECK(context.ticks >= 2110 && context.ticks < 2200);
  CHECK(context.milestones == (ENGAGED|GRUMPY|CALM_RECOVERY|SLEEPY|SLEEPY_SOUND));
  std::printf("[PASS] rollover: 211000 virtual ms, %zu invariant observations\n",context.observations);
}
void scriptValidation() {
  BuddyLifeSimulator buddy; buddy.reset(); current = buddy.snapshot();
  const ScheduledBuddyEvent unordered[] = {{100,E::Tap},{99,E::Hold}};
  const ScheduledBuddyEvent pastEnd[] = {{101,E::Sound}};
  const ScheduledBuddyEvent invalid[] = {{0,static_cast<E>(255)}};
  CHECK(!buddy.runScript(unordered,2,1000));
  CHECK(!buddy.runScript(pastEnd,1,100));
  CHECK(!buddy.runScript(invalid,1,100));
  CHECK(!buddy.runScript(nullptr,1,100));
  CHECK(buddy.now() == 0 && buddy.snapshot().engagement == 0);
  // Ties retain caller order; non-tick final timestamp is exact.
  const ScheduledBuddyEvent tied[] = {{50,E::Tap},{50,E::Hold}};
  CHECK(buddy.runScript(tied,2,151));
  CHECK(buddy.now() == 151 && buddy.snapshot().engagement == 50);
  CHECK(buddy.snapshot().recentInteraction == DiagnosticRecentInteractionType::TouchHold);
}
}
int main() {
  scriptValidation();
  runLife(false);
  runLife(true);
  runRollover();
}

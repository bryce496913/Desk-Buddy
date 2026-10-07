# Buddy life simulator

Link `BuddyLifeSimulator.cpp` and the actual `BehaviorEngine.cpp` with
`DESK_BUDDY_DIAGNOSTICS=1` and `DESK_BUDDY_TEST_LIFE_SIMULATOR=1`.
Do not link hardware engines or other dependency stubs. The host runner provides
an example. BehaviorEngine and the harness clock are global: only one simulator
may run at a time, and `reset()` starts a new run.

```cpp
BuddyLifeSimulator buddy;
buddy.reset();
buddy.advanceBy(30000);
buddy.tap();
buddy.tap();
buddy.advanceBy(45000);
buddy.sound();
auto state = buddy.snapshot();
```

`tap`, `hold`, `sound`, `shortButtonPress`, `longButtonPress`, and
`autonomousEvent` inject production BuddyEvents. No mood rules or reaction
selection tables are reproduced here. Background autonomous scheduling remains
disabled by the diagnostics build; `autonomousEvent()` sends IdleTimeout and
obeys production idle/audio eligibility.

Advancement calls updateBehaviorEngine every 100 ms, with a final shorter step
when necessary. Zero advancement performs one update at the current timestamp.
There is no wall-clock waiting. `advanceTo` means forward modular time: a lower
numeric timestamp advances through wrap, not backwards. Durations are uint32_t
and represent at most one full clock cycle. Stubs use unsigned elapsed time.

Face reactions finish after 1800 virtual ms. Sound activity lasts 600 virtual ms
for non-None reactions and sleep/wake/boot cues. **This does not model exact
buzzer sequence duration**; it only provides deterministic audio eligibility.
Quiet stops reaction audio; system cues remain independent. Last expression and
last reaction sound/mood are historical observations, retained after completion.
Quiet suppression leaves the prior sound record unchanged.

Random tickets are FIFO, modulo the requested range; an empty queue returns
zero/the range minimum. Reset clears tickets and dependency observations. A tiny
hook guarded by both diagnostics and DESK_BUDDY_TEST_LIFE_SIMULATOR clears the
actual engine's autonomous no-repeat history on initialization, so fresh runs
are repeatable within one process. It has no effect on either firmware build.
Snapshots use production getters and read-only diagnostic telemetry; recent
interaction validity means it is still within the production context window.

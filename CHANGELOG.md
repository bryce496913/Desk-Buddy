# Changelog

## [2.0.0] — Unreleased

Personality V2 release preparation. The final release pass must replace
**Unreleased** with the actual release date when v2.0.0 is finalized; no release
date or previous formal v1.0.0 release is asserted here.

### Added

- Four-state BuddyMood: Calm, Engaged, Grumpy, and Sleepy, driven by engagement,
  irritation, awake decay, and meaningful inactivity.
- Affectionate Hold interaction alongside Tap, with a 700 ms Hold threshold.
- Lightweight recent Tap/Hold/Sound context for connected cross-interaction
  reactions within a five-second window.
- Expanded silent autonomous personality: Curious, Daydreaming, SideGlance,
  Bored, SleepyDrift, SuspiciousGlance, ExcitedScanning, and AnnoyedSquint.
- Mood-weighted autonomous pools and mood-aware scheduling ranges: Calm 28–45 s,
  Engaged 12–24 s, Grumpy 20–35 s, Sleepy 45–70 s.
- Procedural expression interpolation for reaction entry, exit, and replacement
  from the currently rendered face.
- Happy bounce, Curious shift, Annoyed tightening, Confused asymmetry, and
  Startled settling micro-animation.
- Mood-aware weighting of the existing reaction-sound variants.
- Quiet Mode: visual personality remains active while production reaction chirps
  are suppressed; system cues remain, and mode persists through sleep/wake.
- Expanded diagnostics for V2 state, forced moods, production Tap/Hold event
  simulation, production-weighted autonomous selection, direct showcases,
  sound mode, variant auditions, and sound-weight telemetry.
- Deterministic Buddy-life simulator, mood-evolution timelines, multi-minute
  state-invariant/rollover simulations, and final V2 lifecycle regression.
- Physical observation guide and an exact-candidate release checklist.

### Changed

- Reaction selection considers pre-event mood, independent Tap/Sound histories,
  and recent cross-interaction context rather than a universal flat ladder.
- Autonomy uses mood-specific pools, weights, and timing instead of the previous
  development baseline's two-expression pool and flat interval.
- Button input distinguishes ShortPress and LongPress: short sleep/wake,
  long Awake Normal/Quiet toggle, long Sleeping wake without a mode toggle.
- The sound vocabulary remains 18 variants; their selection is now mood-aware,
  with immediate-repeat avoidance retained within each family.
- Public documentation describes the V2 behavior and production UF2 installation.

### Preserved

- Raspberry Pi Pico RP2040 hardware and existing GPIO assignments.
- Waveshare ST7789V2 display using Adafruit GFX and Adafruit ST7735/ST7789, with
  fixed-size eye/effect canvases and procedural graphics.
- The same six reaction-sound families with three variants each (18 total).
- Fully local/offline operation: no AI, networking, or voice recognition.
- Button-controlled physical wake; sound alone does not wake Buddy.
- Non-blocking personality logic, rollover-safe timers, diagnostics OFF by
  default, and pinned production/diagnostics Pico CI builds.

Personality V1 was the previous development baseline, not a formal released tag
invented for this history. Curated user-facing notes are in
[RELEASE_NOTES_v2.0.0.md](RELEASE_NOTES_v2.0.0.md).

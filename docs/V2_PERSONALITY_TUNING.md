# Desk Buddy V2 physical personality tuning

This is the observation stage. Firmware behavior and personality values remain
frozen while the existing build is used on the real Desk Buddy. No new features,
expressions, sounds, or hardware changes are part of this phase.

## A. Build identification

Fill this out for each physical session; record the actual flashed firmware
commit, not just the latest repository commit.

| Field | Session record |
| --- | --- |
| Date | |
| Commit | |
| Firmware build / artifact / toolchain | |
| Production / Diagnostics | |
| Hardware / unit notes | |

Baseline reviewed from merged `main`:
[`2b2c9b56cd1d1ff983ff1da172b9c1a784502df0`](https://github.com/bryce496913/Desk-Buddy/commit/2b2c9b56cd1d1ff983ff1da172b9c1a784502df0)
(Pass 13C, after PR #75). The source inventory below was checked against this
commit, rather than inferred from earlier PR descriptions.

Current hardware:

- Raspberry Pi Pico RP2040.
- Waveshare 1.69-inch ST7789V2 display.
- TTP223 touch sensor on GP5 (active HIGH).
- Button on GP7 (`INPUT_PULLUP`, active LOW).
- Passive buzzer on GP15.
- VKLSVAN digital sound sensor on GP26 (active LOW).

CI builds `rp2040:rp2040:rpipico` using Arduino CLI 1.3.1, Arduino-Pico 4.3.1,
Adafruit GFX 1.12.6, Adafruit ST7735/ST7789 1.11.0, and Adafruit BusIO 1.17.4.
Production uses `DESK_BUDDY_DIAGNOSTICS=0`; diagnostics uses `=1`.
The complete 29-suite host baseline passed during this documentation pass.
Check the associated PR's host and both Pico build results before flashing;
[CI workflow](https://github.com/bryce496913/Desk-Buddy/actions/workflows/ci.yml).
Physical V2 observations have not been collected by this preparation pass.

### Exact current personality inventory

Sources: [BehaviorEngine.cpp](../BehaviorEngine.cpp),
[FaceRenderer.cpp](../FaceRenderer.cpp), [Inputs.cpp](../Inputs.cpp),
[SoundEngine.cpp](../SoundEngine.cpp), and [Config.h](../Config.h).
These are recorded values, not proposed tuning targets.

#### Mood thresholds and score effects

| Parameter | Current value |
| --- | --- |
| Score bounds | 0–100, saturating |
| Engaged | Engagement >= 40, provided irritation < 60 |
| Grumpy | Irritation >= 60; takes precedence over Engaged |
| Sleepy | >= 90,000 ms waking inactivity, while Awake, Idle, audio inactive, and below both score thresholds |
| Tap repeat window | 6,000 ms between accepted Taps; exactly 6,000 ms remains in the streak |
| Sound repeat window | 10,000 ms between accepted sounds; exactly 10,000 ms remains in the streak |

| Accepted event while Awake | Engagement delta | Irritation delta |
| --- | --- | --- |
| First Tap in streak | +20 | 0 |
| Second Tap | +25 | 0 |
| Third and subsequent Taps in streak | +5 | +25 |
| Hold | +30 | -15 |
| SoundDetected | +5 | 0 |

Hold does not advance or reset the Tap streak. Touch and sound histories are
independent. Each accepted Tap, Hold, or Sound resets meaningful inactivity and
records recent interaction context. Reactions use the **pre-event mood**, while
longer-lived mood updates from the resulting scores. Physical Sleeping ignores
these personality interactions.

Decay runs every **10,000 ms of awake time**: engagement **-5**, irritation
**-10**, floored at zero. Physical Sleep preserves scores and pauses decay.
Sleep entry and wake reset the decay/inactivity clocks; sleep time produces no
catch-up decay. Wake recomputes mood from preserved scores. Sleepy mood is
separate from physical Sleep; high engagement or irritation can delay Sleepy
beyond 90 seconds of inactivity.

#### Cross-interaction memory

One latest accepted interaction is stored: **TouchTap, TouchHold, or Sound**;
**None** means no recorded interaction. The context window is **5,000 ms,
inclusive**. Expired records remain inspectable with `Recent = No`. Sleep/wake
clears memory. Autonomous reactions and direct showcases do not record a
meaningful interaction or reset its inactivity clock.

#### Autonomous timing and pools

Ranges are inclusive production scheduling ranges, not guaranteed wall-clock
spacing between visible reactions: active reactions/audio and rescheduling can
postpone a start.

| Mood | Timing range | Production pool and base weights |
| --- | --- | --- |
| Calm | 28,000–45,000 ms | Daydreaming 30, SideGlance 25, Curious 25, Bored 20 |
| Engaged | 12,000–24,000 ms | ExcitedScanning 35, Curious 30, SideGlance 20, Daydreaming 15 |
| Grumpy | 20,000–35,000 ms | AnnoyedSquint 40, SuspiciousGlance 35, SideGlance 15, Bored 10 |
| Sleepy | 45,000–70,000 ms | SleepyDrift 45, Daydreaming 30, Bored 20, SideGlance 5 |

Each pool sums to 100. The previous autonomous selection is excluded when
alternatives exist and remaining weights are renormalized; these are base
weights, not independent per-draw percentages. Autonomous expressions are silent.
Background autonomous scheduling is disabled in diagnostics builds.

#### Reaction and input timing

| Parameter | Current value |
| --- | --- |
| Full transient reaction lifetime | 1,800 ms |
| Smoothstep reaction entry | 180 ms, inside the 1,800 ms lifetime |
| Renderer-only smoothstep exit | 220 ms after reaction completion |
| Tap / Hold boundary | 700 ms from debounced press confirmation |
| Button Short / Long boundary | 1,000 ms from debounced press confirmation |
| Input debounce | Stable raw change for >25 ms |

Hold/Long emit once when the threshold is reached; release does not add a second
Tap/Short event. ShortPress toggles physical Sleep/Wake. Awake LongPress toggles
Normal/Quiet; Sleeping LongPress wakes without toggling sound mode.

Entry captures the last rendered per-eye appearance, including lids and apparent
pupil position. Replacements capture the intermediate face instead of resetting
to Normal. Exit is presentation only, not an extension of BehaviorEngine's
reaction lifetime. Animated targets retain their original reaction clock.

Micro-animation limits currently remain: Happy bounce 3 px and bottom-lid change
0.03; Curious shift 5 px; Annoyed top/bottom tightening 0.08/0.03; Confused
outward per-eye movement 3 px; Startled top/bottom settling 0.08/0.05, pupil
growth 2 px, iris growth 1 px, spark cutoff at presentation progress 0.40.
These are maximum extra changes relative to the base expression, not new assets.

#### Sound behavior

**Normal** plays production personality reaction audio. **Quiet** suppresses
personality reaction audio and stops a current reaction sound when entered;
visual reactions, mood, scores, interaction memory, and autonomy continue.
Boot/Sleep/Wake cues remain audible. Mode defaults to Normal on initialization,
persists through physical sleep/wake, and is not stored across a power cycle.
Returning to Normal does not replay suppressed audio.

There are **six reaction sound families**, each with **three existing variants**:
Happy, Curious, Annoyed, Startled, Suspicious, Confused (**18 total variants**).
Mood-aware weighting selects among existing sequences; it does not change notes.
Anti-repeat excludes the last selected variant within that family when another
weighted choice exists. The following are base weights in variant 1/2/3 order;
conditional draws renormalize after anti-repeat exclusion.

| Family | Calm | Engaged | Grumpy | Sleepy |
| --- | --- | --- | --- | --- |
| Happy | 45/35/20 | 25/30/45 | 50/35/15 | 55/35/10 |
| Curious | 40/35/25 | 45/30/25 | 15/30/55 | 15/55/30 |
| Annoyed | 40/30/30 | 35/20/45 | 20/55/25 | 45/40/15 |
| Startled | 45/30/25 | 25/45/30 | 30/20/50 | 30/55/15 |
| Suspicious | 40/35/25 | 25/45/30 | 20/30/50 | 35/20/45 |
| Confused | 40/35/25 | 45/35/20 | 20/30/50 | 20/50/30 |

### Verified physical-tuning diagnostics

Commands below are implemented in [Diagnostics.cpp](../Diagnostics.cpp) and
covered by the existing host diagnostics tests. Open Serial at **115200 baud**
with the diagnostics build. Use lowercase spellings below; spaces and CR/LF
are ignored, and commands can be concatenated. No Serial commands operate in
production mode. Physical Serial behavior still needs device observation.

| Purpose | Commands / exact meaning |
| --- | --- |
| Help | `?`; `e?` production-event help; `a?` autonomous showcase help |
| Force disposition | `mc` Calm, `me` Engaged, `mg` Grumpy, `ms` Sleepy |
| Inspect mood/scores/waking inactivity | `m?` |
| Simulate real production TouchTap / TouchHold | `et` / `eh` |
| One production mood-weighted autonomous selection | `ea` (silent; reports selected behavior) |
| Complete exposed V2 state | `d?`: core, reaction, mood, scores, inactivity, sound mode, recent context, autonomous range/schedule |
| Recent interaction / timing | `i?` / `t?`; `d?` includes range even when scheduling is disabled |
| Sound mode | `q?` inspect, `qn` Normal, `qq` Quiet |
| Choose sound audition variant | `v1`, `v2`, `v3`; `vr` mood-weighted Random (default) |
| Inspect sound selection / weights | `s?` last production/Random selection; `sw` all family weights for current mood |
| Direct reaction showcase | `1` Happy, `2` Curious, `3` Annoyed, `4` Startled, `5` Suspicious, `6` Confused, `7` Daydreaming, `0` Normal |
| Direct autonomous showcase | `a1` Curious, `a2` Daydreaming, `a3` SideGlance, `a4` Bored, `a5` SuspiciousGlance, `a6` AnnoyedSquint, `a7` SleepyDrift, `a8` ExcitedScanning |

Practical isolation examples:

- `mcvr1`: Calm-weighted Random Happy audition. Repeat with `me`, `mg`, `ms`
  to compare mood-aware selection; use `sw` and `s?` for context.
- `v14`, `v24`, `v34`: exact Startled variants 1, 2, 3. Selecting `v1` alone
  changes audition mode; it does not play audio. Variant mode applies to direct
  reaction auditions, not production `et`/`eh`.
- `mc ea`, `me ea`, `mg ea`, `ms ea`: choose from the real production pool for
  that forced mood. Reapply the mood before each isolated comparison and inspect
  `d?`; do not infer natural frequency from how quickly commands are sent.
- `qq et`, `qq eh`: production-path visual reactions without personality chirps;
  check `q?`, then exercise the physical Sleep/Wake button.

Diagnostic forcing is setup, not natural evolution: `me` sets engagement 40,
`mg` sets irritation 60, and other scores are zero; `mc`/`ms` set both scores
zero. It resets the decay anchor without pretending 90 seconds of inactivity
occurred. Forced mood can refresh after real events, decay, or genuine inactivity.

`ea` may replace an active reaction, uses production weights and no-repeat
history, and leaves background scheduling disabled. `a1`–`a8` are isolated,
silent showcases that do not advance production selection history. Direct
`0`–`7` showcases do not record meaningful interactions; explicit sound
auditions **bypass Quiet**. Test Quiet with `et`/`eh` or physical interactions,
not numbered auditions. Forced sound variants do not update the Random
anti-repeat history or `s?` snapshot; use their immediate command response to
identify them. Daydreaming and autonomous showcases are silent.

There is no Serial SoundDetected injection or Serial Sleep/Wake command.
Use environmental sound and the GP7 button. The physical sound sensor suppresses
events while personality reactions/audio are active or Buddy is Sleeping, and
has a 2,500 ms event cooldown and a 250 ms ignore interval after startup, wake,
or Buddy audio ending. Space
cross-interaction sound trials so the real sensor can accept the event, then
check `i?`; a command audition is not a sound-sensor event.

## B. Tuning principle

Tune by feel on the real device. Change one behavioral dimension at a time.
Do not tune multiple unrelated systems from one observation. Prefer small
changes. Do not add new features during this phase. Record the observed problem
before proposing an implementation; source inspection alone is not evidence
that a value is too high, too low, too frequent, or too slow.

## C. Mood-speed evaluation

Use natural interaction and note timing; diagnostic forcing is only for isolating
an expression or comparing moods.

| Transition | Too fast | Right | Too slow | Notes / observed timing |
| --- | --- | --- | --- | --- |
| Calm → Engaged | [ ] | [ ] | [ ] | |
| Engaged → Grumpy | [ ] | [ ] | [ ] | |
| Grumpy → recovery | [ ] | [ ] | [ ] | |
| Calm → Sleepy | [ ] | [ ] | [ ] | |
| Sleepy → alert after interaction | [ ] | [ ] | [ ] | |

- Does Engaged happen so quickly that Calm barely exists?
- Can normal friendly interaction accidentally produce Grumpy?
- Does Grumpy last too long?
- Does Buddy recover too quickly to have personality continuity?
- Does Sleepy happen naturally or almost never?

## D. Grumpy evaluation

Observe first; do not assume Grumpy needs changing.

| Quality | Observation / examples |
| --- | --- |
| Funny / expressive | |
| Believable | |
| Too easy to trigger | |
| Too persistent | |
| Too visually harsh | |
| Too repetitive | |
| Annoying to interact with | |

- Does Grumpy still permit recovery through affection?
- Does a Hold feel like calming/petting Buddy?
- Does Grumpy react defensively without making the user want to stop interacting entirely?
- Are AnnoyedSquint (`a6`) and SuspiciousGlance (`a5`) varied enough in real `mg ea` selections?

## E. Sleepy evaluation

Compare these on the physical display; this visual distinction matters.

| State / behavior | How recognizable? | Distinct from the others? | Notes |
| --- | --- | --- | --- |
| Normal idle drowsiness | | | |
| Daydreaming (`7` or `a2`) | | | |
| Bored (`a4`) | | | |
| SleepyDrift (`a7`, then production `ms ea`) | | | |
| Actual physical Sleep (GP7 ShortPress) | | | |

- Is Sleepy clearly recognizable as a mood?
- Does SleepyDrift look meaningfully different from ordinary drowsiness?
- Does physical Sleep remain obviously different?
- Does Sleepy Buddy move noticeably less often during production desk use?
- Does a sudden accepted environmental sound feel like it wakes Buddy up?

## F. Autonomous behavior evaluation

Observe several **production-selector** events per mood using `mc ea`, `me ea`,
`mg ea`, and `ms ea`, inspecting the reported mood/selection. Use direct `a1`–`a8`
only to inspect individual motion. Judge frequency in a production build with
natural scheduling; diagnostics deliberately disable it.

| Mood | Intended impression | Too frequent | Right frequency | Too rare | Variety good / dominated / repetitive; notes |
| --- | --- | --- | --- | --- | --- |
| Calm | Relaxed, present, not busy | [ ] | [ ] | [ ] | |
| Engaged | Interested, more active, energetic | [ ] | [ ] | [ ] | |
| Grumpy | Watchful, slightly irritated, not constantly hostile | [ ] | [ ] | [ ] | |
| Sleepy | Slow, low-energy, less active | [ ] | [ ] | [ ] | |

## G. Expression-transition evaluation

| Path | Smooth / responsive? | Notes |
| --- | --- | --- |
| Reaction entry | | |
| Reaction exit | | |
| Reaction replacement | | |
| Micro-animation within reaction | | |

Use `1`–`6` for entry/exit, then interrupt a reaction with `et` or `eh` for
replacement. Include partially drowsy/blinking idle → reaction. Repeat using real
Tap/Hold so input feel is assessed too.

- Do lids still visibly pop?
- Do pupils teleport?
- Does iris size visibly snap?
- Does the face ever flash Normal between reactions?
- Are transitions so slow that reactions feel delayed?
- Are transitions so fast that they are barely visible?

The goal is **emotion feels immediate; motion feels smooth**, rather than slow
cinematic animation.

## H. Micro-animation evaluation

| Motion | Too subtle | Good | Too strong | Jittery / mechanical / exaggerated / hard to notice; notes |
| --- | --- | --- | --- | --- |
| Happy bounce (`1`) | [ ] | [ ] | [ ] | |
| Curious pupil shift (`2`) | [ ] | [ ] | [ ] | |
| Annoyed tightening (`3`) | [ ] | [ ] | [ ] | |
| Confused asymmetric movement (`6`) | [ ] | [ ] | [ ] | |
| Startled settling (`4`) | [ ] | [ ] | [ ] | |

## I. Sound evaluation

Use Normal mode (`qn`) and `vr`, audition the same family across all four moods,
then use `v1`/`v2`/`v3` to isolate a troublesome variant. Also listen to accepted
production interactions during normal desk use.

- Does Engaged sound more energetic?
- Does Grumpy sound heavier/skeptical?
- Does Sleepy sound lower-energy except when suddenly startled?
- Does anti-repeat create enough variety?
- Does any single variant become annoying? Record family, mood, variant, context.

Evaluate Quiet with production events, not explicit sound auditions:

- [ ] Visual personality remains complete.
- [ ] Personality chirps are suppressed.
- [ ] Sleep/Wake cues remain appropriate.
- [ ] Quiet survives physical sleep/wake; returning to Normal resumes future chirps.

Notes:

## J. Touch interaction evaluation

| Trial | Intentional / responsive / natural? | Notes |
| --- | --- | --- |
| Tap | | |
| Tap, Tap | | |
| Rapid repeated Tap | | |
| Hold | | |
| Tap → Hold | | |
| Hold → Tap | | |

- Does 700 ms feel right for Hold?
- Does Hold feel intentional rather than accidentally triggered?
- Does Hold feel affectionate?
- Does repeated Tap escalation feel natural?

## K. Cross-interaction evaluation

Use the real sensor, allow reaction/audio suppression and cooldown to clear, and
confirm the accepted context with `i?`. Try both a connected trial inside 5 seconds
and a separated trial beyond 5 seconds. Re-establish the starting mood/context
for each isolated diagnostic trial.

| Sequence | Feels connected? | Appears to remember? | Notes / gap / accepted event |
| --- | --- | --- | --- |
| Tap → environmental sound | | | |
| Hold → environmental sound | | | |
| Environmental sound → Tap | | | |
| Environmental sound → Hold | | | |
| Separated sequence (>5,000 ms) | | | |

- Do these feel like connected interactions?
- Does Buddy appear to remember what just happened?
- Is the 5-second context window perceptible without feeling artificial?

## L. Overall character questions

After extended normal use:

- Does Buddy feel less deterministic than V1?
- Can I tell what mood it is in without diagnostics?
- Does it surprise me occasionally without becoming annoying?
- Does it feel responsive when I interact with it?
- Does it also feel alive when I leave it alone?
- Does any behavior happen so frequently that I notice the algorithm?
- Does any behavior almost never appear?
- Does Quiet Mode feel useful?
- Which one change would improve the personality the most?

## Observation period

Use this unchanged build across multiple normal sessions, preferably across
several days: focused work, casual interaction, and long unattended stretches.
Include both short diagnostic comparison sessions and longer production-mode desk
use. Keep firmware/hardware identification with each log entry.

Diagnostics isolate behavior. Normal use judges personality. Five minutes of
forced commands is not enough to judge natural mood speed, frequency, or
character continuity. Do not replace natural observation with repeated commands.
Collect and review observations together before choosing a single tuning area.

## Reusable tuning log

Copy an entry for each observation. Severity is **Minor**, **Noticeable**, or
**Major**. A proposed code change is not required; capture the problem first.

### Observation 1

```text
Observation:
Context:
Current mood:
Action/event:
What Buddy did:
Expected/desired feel:
Actual feel:
Severity: Minor / Noticeable / Major
Possible tuning area:
```

### Observation 2

```text
Observation:
Context:
Current mood:
Action/event:
What Buddy did:
Expected/desired feel:
Actual feel:
Severity: Minor / Noticeable / Major
Possible tuning area:
```

### Observation 3

```text
Observation:
Context:
Current mood:
Action/event:
What Buddy did:
Expected/desired feel:
Actual feel:
Severity: Minor / Noticeable / Major
Possible tuning area:
```

## Freeze declaration

V2 personality feature development is frozen during physical tuning.

No threshold, timing, weighting, sound, expression, or reaction-policy changes
should be merged until physical observations have been collected and reviewed
together.

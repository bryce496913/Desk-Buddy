# Desk Buddy — Personality V2

[![CI](https://github.com/bryce496913/Desk-Buddy/actions/workflows/ci.yml/badge.svg)](https://github.com/bryce496913/Desk-Buddy/actions/workflows/ci.yml)

Desk Buddy is a fully local, offline Raspberry Pi Pico RP2040 companion with
animated procedural eyes, Tap and Hold interaction, environmental-sound
reactions, longer-lived moods, recent cross-interaction memory, autonomous
personality, mood-aware sounds, Quiet Mode, and button sleep/wake. Optional
Serial diagnostics support development and physical observation.

Everything runs on the RP2040: **no AI, networking, or voice recognition**. The
digital sound sensor detects a threshold event; it does not interpret speech.

V2 behavior is protected by host regressions and real production/diagnostics
Pico CI builds. Physical validation applies to the exact firmware commit and
hardware used: record the release candidate's device results using the
[physical tuning guide](docs/V2_PERSONALITY_TUNING.md) and
[release checklist](RELEASE_CHECKLIST.md) before publishing.

## Hardware

The current Desk Buddy hardware uses:

- Raspberry Pi Pico / RP2040
- Waveshare 1.69-inch ST7789V2 240 x 280 LCD
- TTP223 capacitive touch sensor
- VKLSVAN digital sound sensor
- Passive buzzer
- Push button

### Pin map

The firmware pin assignments in [`Config.h`](Config.h) are:

| Pico GPIO | Connection |
| --- | --- |
| GP5 | TTP223 `OUT` |
| GP7 | Push button |
| GP15 | Passive buzzer |
| GP17 | LCD `CS` |
| GP18 | LCD `SCK` / `CLK` |
| GP19 | LCD `MOSI` |
| GP20 | LCD `DC` |
| GP21 | LCD `RST` |
| GP22 | LCD backlight |
| GP26 | VKLSVAN digital sound-sensor output |

### Electrical notes

- Wire the button between **GP7 and GND**. The firmware configures GP7 as
  `INPUT_PULLUP`, so a press is active LOW.
- The VKLSVAN digital output on GP26 is also active LOW (HIGH when idle, LOW on
  a detected sound).
- **RP2040 GPIO is not 5 V tolerant.** Power and configure the digital sound
  sensor so its output remains within the Pico GPIO voltage limits.

## Display stack

The display code uses **Adafruit GFX** and **Adafruit ST7735/ST7789**, not
TFT_eSPI. It initializes the controller as a 240 x 280 ST7789 and applies
rotation `1` for the Desk Buddy orientation, producing the firmware's 280 x 240
logical drawing area.

## Firmware architecture

```text
DeskBuddy.ino
    ↓
Inputs / SoundSensor
    ↓
BuddyEvent
    ↓
BehaviorEngine
    ↓
FaceRenderer + SoundEngine
```

| Module | Responsibility |
| --- | --- |
| `DeskBuddy.ino` | Initializes modules and coordinates the non-blocking main loop. |
| `BehaviorEngine` | Owns Awake/Sleeping core state, BuddyMood, independent interaction histories, recent cross-interaction context, reaction selection, weighted autonomous selection/timing, and Quiet Mode. |
| `Inputs` | Debounces touch/button input and classifies Tap/Hold and ShortPress/LongPress. |
| `FaceRenderer` | Draws procedural eyes, idle motion, expressions, entry/exit interpolation, micro-animation, and the sleeping face/backlight. |
| `SoundEngine` | Plays non-blocking buzzer sequences and selects mood-weighted reaction variants with immediate-repeat avoidance. |
| `SoundSensor` | Captures active-LOW triggers and filters cooldown, self-audio, reaction, and sleep suppression. |
| `Diagnostics` | Optional Serial controls for V2 state, production-event simulation, expression showcases, and sound auditions. |
| `Config` | Defines the diagnostics build switch, GPIO assignments, and display geometry. |

## Personality V2

### Moods

Buddy has four longer-lived dispositions, separate from physical sleep and
short-lived facial reactions:

| Mood | Character |
| --- | --- |
| Calm | Relaxed and present. |
| Engaged | Interested and more energetic. |
| Grumpy | Watchful and irritated, with affectionate recovery available. |
| Sleepy | Low-energy after prolonged waking inactivity. |

Interaction accumulates engagement and irritation; scores decay during awake
time. Inactivity can produce Sleepy when score precedence and idle/audio
eligibility permit it. Mood influences reaction selection, autonomous behavior,
event timing, and sound-variant selection. Reactions use the mood on arrival,
before that interaction changes the longer-lived disposition.

### Tap and Hold

A **Tap** retains repeated-interaction escalation, now interpreted through mood
and recent context. Taps have an independent rolling **6-second** repeat window;
there is no single expression ladder for every mood.

A **Hold** is an affectionate/petting interaction: it increases engagement and
reduces irritation. The current threshold is **700 ms** from the debounced
press. Hold emits once; releasing it does not add a Tap. It does not advance the
Tap streak.

### Recent cross-interaction memory

Buddy stores one recent Tap, Hold, or accepted Sound with its timestamp. The
context is valid for **5 seconds, inclusive**, allowing sequences such as
Tap → Sound and Sound → Hold to produce connected reactions. It is lightweight
interaction context, not persistent memory. Physical sleep/wake clears it.

Accepted sounds also have their own **10-second** repeat history. Raw sensor
edges are not accepted interactions when filtering suppresses them.

### Autonomous personality

When Awake and eligible for an idle reaction, Buddy silently chooses among:
**Curious, Daydreaming, SideGlance, Bored, SleepyDrift, SuspiciousGlance,
ExcitedScanning, and AnnoyedSquint**.

Mood controls both behavior weighting and scheduled frequency:

| Mood | Current scheduling range |
| --- | --- |
| Calm | 28–45 seconds |
| Engaged | 12–24 seconds |
| Grumpy | 20–35 seconds |
| Sleepy | 45–70 seconds |

Ranges are inclusive. Active reactions/audio and rescheduling can postpone a
visible event; these are not guaranteed intervals between expressions. The
previous autonomous choice is excluded when alternatives exist. Autonomous
activity does not count as meaningful interaction or keep resetting inactivity.

### Expression polish

V2 uses procedural interpolation for smooth reaction entry, exit, and replacement
from the currently rendered face. Entry takes **180 ms** within the **1800 ms**
reaction lifetime; the renderer's return to idle takes **220 ms** afterward.

Small expression-specific motions include Happy bounce, Curious pupil shift,
Annoyed tightening, Confused asymmetry, and Startled settling. SleepyDrift and
ExcitedScanning remain animated. Rendering reuses the fixed eye/effect canvases;
no new graphics or asset sheets are required.

### Mood-aware sounds

There are **18 existing reaction variants: six families × three variants**.
Happy, Curious, Annoyed, Startled, Suspicious, and Confused each retain their
three sequences. Mood weights which variant is more likely; immediate-repeat
avoidance remains within each family. V2 changes selection rather than adding
a new sound vocabulary.

## Quiet Mode and sleep/wake

| GP7 button action | Result |
| --- | --- |
| ShortPress while Awake | Enter physical Sleep. |
| ShortPress while Sleeping | Wake. |
| LongPress while Awake | Toggle Normal ↔ Quiet. |
| LongPress while Sleeping | Wake without changing sound mode. |

The LongPress threshold is **1000 ms** from the debounced press. ShortPress is
recognized on release; LongPress emits once at the threshold, with no extra
ShortPress on release.

Quiet keeps visual reactions and mood evolution, suppresses production
personality chirps, and stops a current personality reaction sound. Boot and
Sleep/Wake cues remain. Quiet survives physical sleep/wake, resets to Normal
after reboot, and does not replay suppressed sounds when Normal is restored.

Physical Sleep closes the eyes, displays an animated `Z`, dims the backlight,
and plays the sleep cue. Wake restores the display and plays the wake cue.
Scores are preserved during Sleep, decay pauses, and wake starts fresh awake
inactivity/decay clocks and recomputes mood. Touch/sound histories and recent
context are cleared on sleep entry; wake clears context and sound history.

**The sound sensor does not wake Buddy; the button is the wake control.**
Sleepy mood is distinct from physical Sleep.

## VKLSVAN sound detection

GP26 uses an active-LOW falling-edge interrupt. Filtering applies a **2500 ms**
cooldown after accepted sounds, suppression while buzzer audio or a personality
reaction is active, suppression during physical Sleep, and **250 ms** settling
at startup, after wake, and after Buddy audio ends.

For troubleshooting, check the active-LOW signal voltage and allow reaction,
audio, cooldown, and settling to finish before the next sound trial.

## Install Personality V2

When the [v2.0.0 GitHub Release](https://github.com/bryce496913/Desk-Buddy/releases/tag/v2.0.0)
is published, its planned assets are:

- `DeskBuddy-v2.0.0.uf2` — production firmware, **diagnostics disabled**.
- `DeskBuddy-v2.0.0.uf2.sha256` — SHA-256 checksum sidecar.
- `BUILD_INFO.txt` — exact source commit and build/dependency metadata.

Installation:

1. Download `DeskBuddy-v2.0.0.uf2` from that release.
2. Hold **BOOTSEL** while connecting/resetting the Pico to expose **RPI-RP2**.
3. Copy the UF2 to **RPI-RP2**.
4. The Pico reboots into Personality V2.

To verify the download where `sha256sum` is available, put the UF2 and sidecar
in the same directory and run `sha256sum -c DeskBuddy-v2.0.0.uf2.sha256`.
This documentation pass does not create the release or its assets.

### GitHub Actions production artifact

Successful production CI jobs expose `desk-buddy-production-firmware`, containing
`DeskBuddy-production.uf2` and `BUILD_INFO.txt`. This current Actions artifact is
separate from the planned versioned release assets; the final packaging pass
renames the verified production UF2 without changing its bytes and generates
the checksum sidecar. Use the artifact's metadata to identify its exact commit.

## Build from source

CI pins and verifies these dependencies:

| Component | Version |
| --- | --- |
| Arduino CLI | **1.3.1** |
| Arduino-Pico core (`rp2040:rp2040`) | **4.3.1** |
| Adafruit GFX Library | **1.12.6** |
| Adafruit ST7735 and ST7789 Library | **1.11.0** |
| Adafruit BusIO | **1.17.4** |
| Target FQBN | **`rp2040:rp2040:rpipico`** |

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) is the source of truth for
build versions and artifact packaging.

### Arduino IDE

1. Add the Arduino-Pico index URL to **Preferences > Additional Boards Manager URLs**:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`.
2. Install Arduino-Pico **4.3.1** and the three library versions listed above.
3. Copy the repository's root `DeskBuddy.ino`, `.cpp`, and `.h` files into a
   directory named `DeskBuddy`, so the sketch and directory names match.
4. Open that `DeskBuddy.ino`, select **Raspberry Pi Pico**, and compile/upload.

### Arduino CLI

From the repository root, with Arduino CLI 1.3.1 installed:

```sh
PICO_INDEX_URL=https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core update-index --additional-urls "$PICO_INDEX_URL"
arduino-cli core install rp2040:rp2040@4.3.1 --additional-urls "$PICO_INDEX_URL"
arduino-cli lib install "Adafruit BusIO@1.17.4"
arduino-cli lib install "Adafruit GFX Library@1.12.6" --no-deps
arduino-cli lib install "Adafruit ST7735 and ST7789 Library@1.11.0" --no-deps

PICO_SKETCH_DIR="$(mktemp -d)/DeskBuddy"
mkdir -p "$PICO_SKETCH_DIR"
find . -maxdepth 1 -type f \( -name 'DeskBuddy.ino' -o -name '*.cpp' -o -name '*.h' \) \
  -exec cp '{}' "$PICO_SKETCH_DIR/" \;
arduino-cli compile --fqbn rp2040:rp2040:rpipico --warnings all \
  --build-property 'compiler.cpp.extra_flags=-DDESK_BUDDY_DIAGNOSTICS=0' \
  "$PICO_SKETCH_DIR"
```

For diagnostics, repeat compilation with `DESK_BUDDY_DIAGNOSTICS=1` in the
extra-flags property. Diagnostics default OFF in `Config.h`; no source edit is
needed to switch build modes.

## V2 diagnostics

In a diagnostics build, open Serial at **115200 baud**. Startup prints help.
Commands can be concatenated; spaces and CR/LF are ignored. Use these lowercase
spellings:

| Command | Purpose |
| --- | --- |
| `?` | Main help. |
| `d?` | V2 state: core/reaction, mood/scores/inactivity, sound mode, recent context, autonomous timing/range. |
| `m?` | Mood, scores, waking inactivity (zero while physically Sleeping). |
| `mc`, `me`, `mg`, `ms` | Force Calm, Engaged, Grumpy, Sleepy for isolated testing. |
| `et`, `eh` | Simulate real production Tap/Hold events; mutate scores/history and obey Quiet/Sleep. |
| `ea` | One real mood-weighted autonomous selection; reports the chosen behavior. |
| `e?` | Production-event help. |
| `i?` | Recent interaction type, age, and validity. |
| `t?` | Autonomous timing telemetry; background scheduling is disabled in diagnostics. |
| `a1`–`a8`, `a?` | Direct silent autonomous showcase and help (mapping below). |
| `q?`, `qn`, `qq` | Inspect sound mode, set Normal, set Quiet. |
| `s?`, `sw` | Last production/Random sound selection; all family weights for current mood. |
| `v1`, `v2`, `v3`, `vr` | Exact sound variant 1/2/3 or mood-weighted Random for subsequent numbered auditions. |
| `0`–`7` | Direct reaction showcase (mapping below). |

| Showcase | Mapping |
| --- | --- |
| Reaction `0`–`7` | 0 Normal, 1 Happy, 2 Curious, 3 Annoyed, 4 Startled, 5 Suspicious, 6 Confused, 7 Daydreaming. |
| Autonomous `a1`–`a8` | a1 Curious, a2 Daydreaming, a3 SideGlance, a4 Bored, a5 SuspiciousGlance, a6 AnnoyedSquint, a7 SleepyDrift, a8 ExcitedScanning. |

`a1`–`a8` are isolated showcases; `ea` uses production weighting and no-repeat
history, may replace a reaction, and does not enable background scheduling.
Judge natural event frequency with a production build.

Direct reaction auditions do not record interactions and **bypass Quiet** for
explicit sound testing. Use `et`/`eh` or physical interaction to test Quiet.
`v1` alone selects audition mode, rather than playing sound: for example, `v14`
auditions Startled variant 1. `mcvr1` auditions Calm-weighted Random Happy.
Forced variants do not update the Random selection snapshot/history. Daydreaming
and autonomous showcases are silent. There is no Serial SoundDetected injection
or Sleep/Wake command; use the real sensor and GP7 button.

## Tests and CI

Run the full strict host suite:

```sh
./tests/run_host_tests.sh
git diff --check
```

Coverage includes production BehaviorEngine, touch/button gesture boundaries,
SoundSensor filtering, mood state/lifecycle, cross-interaction memory, weighted
autonomy and timing, mood-aware sound selection, Quiet audio policy, V2
command parsing and event simulation, and renderer expressions/interpolation,
entry/exit, micro-motion, lifecycle, and rollover behavior.

The reusable [Buddy-life simulator](tests/BuddyLifeSimulator.md) exercises the
actual BehaviorEngine using deterministic virtual time and production events.
Mood-evolution scenarios and multi-minute long runs check lifecycle and state
invariants instantly, including rollover and Normal/Quiet parity. A release-level
regression follows a representative natural V2 lifecycle without forcing moods.
The simulator's sound stub models eligibility, not exact buzzer duration;
separate sound integration tests exercise real sequence playback.

GitHub Actions runs host tests plus real production and diagnostics Pico builds
on pull requests and pushes to `main`, using the pinned dependencies above.
The badge reports this workflow's status.

## Repository structure

```text
DeskBuddy.ino               Main sketch and module coordination
Config.h                    GPIO assignments, display geometry, build switch
BehaviorEngine.{h,cpp}      Core state, mood, events, selection, timing, Quiet
Inputs.{h,cpp}              Touch/button debounce and gesture classification
SoundSensor.{h,cpp}         VKLSVAN interrupt capture and filtering
FaceRenderer.{h,cpp}        Procedural eyes, interpolation, micro-animation
SoundEngine.{h,cpp}         Passive-buzzer playback and mood-weighted variants
Diagnostics.{h,cpp}         Optional V2 Serial controls and telemetry
tests/                      Host tests, simulator, Arduino/display shims
docs/V2_PERSONALITY_TUNING.md  Source inventory and physical observation guide
CHANGELOG.md                Version history; release date pending
RELEASE_NOTES_v2.0.0.md      Curated release notes
RELEASE_CHECKLIST.md         Exact-candidate release gates
.github/workflows/ci.yml    Host tests, Pico builds, production artifact
```

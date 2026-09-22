# Desk Buddy Personality V1

[![CI](https://github.com/bryce496913/Desk-Buddy/actions/workflows/ci.yml/badge.svg)](https://github.com/bryce496913/Desk-Buddy/actions/workflows/ci.yml)

Desk Buddy is a small, Raspberry Pi Pico-based expressive desk companion. The
Personality V1 firmware gives it animated LCD eyes, capacitive-touch interaction,
environmental-sound reactions, passive-buzzer personality sounds, push-button
sleep/wake, and autonomous idle behavior.

Everything runs locally on the RP2040. This version has **no AI, networking, or
voice recognition**; the sound sensor detects a threshold event rather than
interpreting audio.

The current Personality V1 behavior has passed real-world functional testing on
the physical Desk Buddy hardware and is protected by the checked-in host
regression tests and CI builds. This is a practical validation statement, not a
certification or reliability guarantee.

## Hardware

The physically validated build uses:

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
| `BehaviorEngine` | Owns awake/sleep state, interaction histories, reaction selection, and autonomous personality scheduling. |
| `FaceRenderer` | Draws normal, reaction, and sleeping eyes; manages idle eye motion, blinks, and backlight transitions. |
| `SoundEngine` | Plays non-blocking buzzer sequences and chooses reaction-sound variants. |
| `SoundSensor` | Captures active-LOW sound triggers and filters events that should not reach the behavior engine. |
| `Inputs` | Debounces the capacitive touch input and push button and emits press events. |
| `Diagnostics` | When compiled in, parses Serial commands for reaction and sound-variant tuning. |
| `Config` | Defines the diagnostic build switch, GPIO assignments, and display geometry. |

## Personality behavior

### Touch ladder

Touches accepted while awake follow a rolling **6-second** history:

```text
1st recent touch  → Happy
2nd recent touch  → Curious
3rd+ recent touch → Annoyed
```

After more than six seconds without another touch, the next touch starts again
at Happy.

### Sound ladder

Accepted sound events follow a rolling **10-second** history:

```text
1st accepted sound  → Startled
2nd accepted sound  → Suspicious
3rd+ accepted sound → Confused
```

The repeat window is based on accepted `SoundDetected` events. Raw noises and
sensor edges are not equivalent to accepted events because the sound-sensor
cooldown and suppression rules can reject them.

### Sound vocabulary

Each of the six interactive personalities—Happy, Curious, Annoyed, Startled,
Suspicious, and Confused—has **three sound variants**. Production firmware
chooses among them randomly while preventing the same personality's variant
from repeating immediately. See [`SoundEngine.cpp`](SoundEngine.cpp) for the
sequence definitions and selection logic.

### Autonomous personality

While Buddy is awake and genuinely idle, an autonomous reaction is scheduled
approximately every **20–40 seconds**. Buddy silently performs either:

- Curious
- Daydreaming

These autonomous expressions do not increment or reset the real touch and
accepted-sound interaction histories. User interaction reschedules the idle
timer.

## Sleep and wake

The button on GP7 toggles the power state:

```text
press while awake    → sleep
press while sleeping → wake
```

Sleep dims the LCD backlight, closes the eyes, displays an animated `Z`, and
plays the sleep sound. Wake restores the backlight, animates the eyes, and plays
the wake sound. The sound sensor cannot wake Buddy; only the button does so.
Touch and sound histories are reset on entry to sleep, and sound history is
reset again on wake so stale interactions do not carry into a new awake period.

## VKLSVAN sound detection

The VKLSVAN input uses an active-LOW **falling-edge interrupt**. To reduce false
or self-triggered reactions, the firmware applies:

- a **2500 ms** cooldown after each accepted sound event;
- suppression while Buddy's buzzer audio is active;
- suppression while a face reaction is active;
- suppression while Buddy is sleeping; and
- short settling periods at startup, after wake, and after Buddy audio ends.

If sound reactions are missing during troubleshooting, first check the active-LOW
signal voltage and then allow the current reaction, buzzer sequence, cooldown,
and settling interval to finish before testing again.

## Build and upload

CI validates the following pinned environment:

| Component | Version |
| --- | --- |
| Arduino-Pico core (`rp2040:rp2040`) | **4.3.1** |
| Adafruit GFX Library | **1.12.1** |
| Adafruit ST7735 and ST7789 Library | **1.11.0** |
| Target FQBN | **`rp2040:rp2040:rpipico`** |

Adafruit BusIO is installed as a library dependency; the workflow does **not**
pin it explicitly. The CI source of truth for build versions is
[`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Arduino IDE

1. Add the Arduino-Pico package index URL to **Preferences > Additional Boards
   Manager URLs**:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Install version **4.3.1** of the `rp2040` platform in Boards Manager.
3. Install **Adafruit GFX Library 1.12.1** and **Adafruit ST7735 and ST7789
   Library 1.11.0** in Library Manager. Allow the IDE to install required
   dependencies such as Adafruit BusIO.
4. Open `DeskBuddy.ino`, select **Raspberry Pi Pico**, choose the correct port,
   and compile/upload.

### Arduino CLI

With `arduino-cli` installed, reproduce the dependency setup used by CI:

```sh
INDEX_URL=https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

arduino-cli core update-index --additional-urls "$INDEX_URL"
arduino-cli core install rp2040:rp2040@4.3.1 --additional-urls "$INDEX_URL"
arduino-cli lib install \
  "Adafruit GFX Library@1.12.1" \
  "Adafruit ST7735 and ST7789 Library@1.11.0"
```

Arduino requires the sketch directory and primary `.ino` file to share a base
name. Because this repository is named `Desk-Buddy` while the sketch is
`DeskBuddy.ino`, either open the file in the IDE or stage the root `.ino`, `.cpp`,
and `.h` files in a directory named `DeskBuddy` as CI does. Then compile:

```sh
arduino-cli compile --fqbn rp2040:rp2040:rpipico /path/to/DeskBuddy
```

## Diagnostics

Diagnostics are compile-time disabled by default in `Config.h`:

```cpp
#define DESK_BUDDY_DIAGNOSTICS 0
```

For a development build, set the macro to `1` locally or pass
`-DDESK_BUDDY_DIAGNOSTICS=1` as a compiler flag (the CI diagnostics build uses
the latter). Connect a Serial monitor at **115200 baud**. Diagnostics print help
at startup and accept one-character reaction commands:

| Command | Reaction |
| --- | --- |
| `0` | Normal |
| `1` | Happy |
| `2` | Curious |
| `3` | Annoyed |
| `4` | Startled |
| `5` | Suspicious |
| `6` | Confused |
| `7` | Daydreaming |
| `?` | Help |

Sound-variant commands set the mode used by subsequent reaction commands:

| Command | Selection |
| --- | --- |
| `v1` | Force variant 1 |
| `v2` | Force variant 2 |
| `v3` | Force variant 3 |
| `vr` | Restore random selection |

Forced variants exist for tuning and deterministic testing only. Normal
production builds keep randomized selection with immediate-repeat avoidance.
Daydreaming is intentionally silent.

## Tests

Run the complete local host regression suite from the repository root:

```sh
./tests/run_host_tests.sh
```

The script compiles with `g++` and exercises:

- the production `BehaviorEngine`;
- `SoundSensor` event filtering;
- `FaceRenderer` timing across the `millis()` rollover;
- the diagnostics-enabled `BehaviorEngine`;
- the diagnostics Serial parser; and
- deterministic diagnostic sound-variant selection.

## Continuous integration

GitHub Actions workflow [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
runs on pull requests and pushes to `main`. It verifies:

1. the host regression tests;
2. a real production Raspberry Pi Pico compile; and
3. a real diagnostics-enabled Raspberry Pi Pico compile.

Both firmware builds target `rp2040:rp2040:rpipico` using the pinned core and
display-library versions above. The badge at the top of this README reports this
workflow's status.

## Repository structure

```text
DeskBuddy.ino             Main sketch and module coordination
Config.h                  GPIO assignments, display geometry, build switch
BehaviorEngine.{h,cpp}    State, event mapping, and personality scheduling
Inputs.{h,cpp}            Touch and button input/debouncing
SoundSensor.{h,cpp}       VKLSVAN interrupt capture and event filtering
FaceRenderer.{h,cpp}      ST7789 eye and expression rendering
SoundEngine.{h,cpp}       Passive-buzzer sequences and variant selection
Diagnostics.{h,cpp}       Optional Serial diagnostic interface
tests/                    Host regression suite and Arduino test shim
.github/workflows/ci.yml  Host tests and production/diagnostics Pico builds
```

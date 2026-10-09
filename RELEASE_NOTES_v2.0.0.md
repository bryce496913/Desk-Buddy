# v2.0.0 — Personality V2

## What changed

Desk Buddy now carries a longer-lived disposition between interactions. Mood,
recent context, varied autonomous expressions, smoother eyes, and mood-aware
sound selection give the existing hardware a broader procedural personality.
Everything remains fully local and offline: no AI, networking, or voice
recognition.

## Personality and moods

Calm, Engaged, Grumpy, and Sleepy influence how Buddy reacts and spends idle time.
Interactions build engagement and irritation; awake time lets those scores
recover, and prolonged inactivity can make Buddy Sleepy. Sleepy is a mood,
separate from physical Sleep.

## Interaction

Tap escalation is now mood-aware. Hold for 700 ms to give an affectionate/petting
interaction that increases engagement and reduces irritation. A lightweight
five-second memory of the latest Tap, Hold, or accepted Sound makes connected
sequences such as Tap → Sound and Sound → Hold feel related.

## Autonomous behavior

Buddy can silently perform Curious, Daydreaming, SideGlance, Bored, SleepyDrift,
SuspiciousGlance, ExcitedScanning, and AnnoyedSquint. Mood influences both the
choice and timing: Engaged is scheduled more often, Sleepy less often. Autonomous
activity does not count as user attention or reset meaningful inactivity.

## Visual polish

Reaction entry, exit, and replacement interpolate smoothly from the visible
face. Small procedural motions add Happy bounce, Curious pupil shift, Annoyed
tightening, Confused asymmetry, and Startled settling, while animated autonomous
expressions continue moving. The existing display and graphics approach remain.

## Sound

The same **18 reaction variants** remain: six families, three variants each.
Mood changes which variant is more likely, with immediate-repeat avoidance
within each family. This release broadens sound selection rather than adding
a new vocabulary of buzzer sequences.

## Quiet Mode

Short button presses control physical sleep/wake. Hold the button for **1000 ms
while Awake** to toggle Normal/Quiet. Quiet keeps visual reactions and mood
behavior but suppresses personality chirps; boot and sleep/wake cues remain.
Quiet survives sleep/wake and returns to Normal after reboot. A long press while
Sleeping wakes Buddy without changing sound mode. Environmental sound does not
wake physically Sleeping Buddy.

## Diagnostics/testing

Optional diagnostics expose mood, scores, recent interactions, sound mode, and
complete V2 state. They support direct showcases, real Tap/Hold event simulation,
one production mood-weighted autonomous selection, and exact or weighted-random
sound auditions.

Regression coverage includes deterministic mood evolution, multi-minute Buddy
life, state invariants, rollover, Normal/Quiet parity, and a representative release
lifecycle. Production and diagnostics builds are checked using pinned Pico and
display dependencies. The normal-use production UF2 has **diagnostics disabled**.

## Installation

The versioned release package is intended to contain:

- `DeskBuddy-v2.0.0.uf2` — production firmware.
- `DeskBuddy-v2.0.0.uf2.sha256` — SHA-256 checksum.
- `BUILD_INFO.txt` — source commit and build information.

Once the v2.0.0 GitHub Release is published:

1. Download `DeskBuddy-v2.0.0.uf2` from that release.
2. Hold **BOOTSEL** while connecting/resetting the Pico to expose **RPI-RP2**.
3. Copy the UF2 to **RPI-RP2**.
4. The Pico reboots into Personality V2.

Verify the checksum with `sha256sum -c DeskBuddy-v2.0.0.uf2.sha256` where available.
Use the production UF2 for normal desk use; the diagnostic build is for testing.

## Hardware compatibility

Hardware and pins are unchanged: Raspberry Pi Pico RP2040, Waveshare 1.69-inch
ST7789V2, TTP223 on GP5, button on GP7, passive buzzer on GP15, and VKLSVAN digital
sound input on GP26. RP2040 GPIO remains 3.3 V only and is not 5 V tolerant.
The display stack remains Adafruit GFX plus Adafruit ST7735/ST7789.

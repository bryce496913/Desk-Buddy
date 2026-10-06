# Changelog

## [Unreleased] — preparing 1.0.0

The release date will be recorded when v1.0.0 is finalized. The README records
physical validation of Personality V1; release checks must be repeated for the
exact release commit.

### Added

- Animated ST7789 eyes with blinking, wandering pupils, idle drowsiness,
  procedural personality expressions, and a sleeping face with an animated Z.
  Rendering reuses fixed-size eye and effect canvases allocated at startup.
- Touch reactions progress from Happy to Curious to Annoyed using a six-second
  rolling interaction history.
- Accepted sound reactions progress from Startled to Suspicious to Confused
  using a ten-second rolling history.
- Three sound variants for each of the six interactive personalities, with
  non-blocking playback and immediate-repeat avoidance.
- Silent autonomous Curious and Daydreaming expressions after a randomized
  approximately 20–40-second eligible idle interval.
- GP7 button sleep/wake, dedicated sounds, dimmed/restored backlight, touch and
  sound history resets on sleep, and a sound history reset on wake.
- Active-LOW VKLSVAN digital sound input on GP26, a 2500 ms accepted-event
  cooldown, suppression during self-audio, reactions and sleep, and 250 ms
  settling at startup, after wake and after audio ends.
- Optional Serial diagnostics with direct expression commands, deterministic
  `v1`/`v2`/`v3` sound selection and `vr` random mode. Diagnostics default off.
- Modular firmware, a non-blocking main loop, rollover-safe timing, and six
  host regression suites. CI validates production and diagnostics Pico builds
  with Arduino-Pico 4.3.1, Adafruit GFX 1.12.6, Adafruit ST7735/ST7789 1.11.0
  and Adafruit BusIO 1.17.4.
- Production-only UF2 packaging in CI as `desk-buddy-production-firmware`,
  containing `DeskBuddy-production.uf2` and generated `BUILD_INFO.txt` source
  and dependency metadata. Confirm the downloadable artifact in a successful
  Actions run after this workflow change is merged.

### V1 scope

V1 runs locally on RP2040. Networking, AI, speech recognition, analog sound
amplitude analysis and sound-based wake are deliberately outside its scope.
Only the button wakes Buddy.

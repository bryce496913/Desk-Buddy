# Personality V1 release checklist

Complete this checklist for the exact commit intended for v1.0.0. The README
records prior physical validation; repeat the smoke test on the release build.
This checklist does not create a tag or publish a release.

## Source

- [ ] `main` is up to date.
- [ ] No unmerged V1 fixes remain.
- [ ] `DESK_BUDDY_DIAGNOSTICS` defaults to `0` and production compiles with `0`.
- [ ] No temporary debug changes remain.

## Tests

- [ ] `./tests/run_host_tests.sh` passes all six suites.
- [ ] GitHub Actions `host-tests` passes.
- [ ] Production Pico compile passes for `rp2040:rp2040:rpipico`.
- [ ] Diagnostics Pico compile passes for the same board.
- [ ] Dependency verification passes: Arduino-Pico 4.3.1, Adafruit GFX 1.12.6,
      Adafruit ST7735/ST7789 1.11.0 and Adafruit BusIO 1.17.4.

## Hardware

- [ ] Boot display works on the documented Pico/ST7789 hardware.
- [ ] Touch ladder gives Happy → Curious → Annoyed within the rolling six-second
      history; a pause longer than six seconds restarts at Happy.
- [ ] Accepted sound ladder gives Startled → Suspicious → Confused within the
      rolling ten-second history; allow reactions, audio, cooldown and settling
      to finish between events. A pause longer than ten seconds restarts it.
- [ ] Eligible idle time produces silent autonomous Curious and Daydreaming.
- [ ] GP7 sleep/wake works, with sounds, closed eyes/Z, backlight changes and
      fresh interaction histories; sound alone does not wake Buddy.
- [ ] Buzzer does not self-trigger VKLSVAN after audio and settling finish.
- [ ] No obvious display corruption occurs during interaction or sleep/wake.

## Release artifact

- [ ] Successful production job exposes `desk-buddy-production-firmware`.
- [ ] Download and inspect the artifact: exactly one nonempty production UF2
      (`DeskBuddy-production.uf2`) and `BUILD_INFO.txt` are present.
- [ ] UF2 is from the diagnostics-off production build, never the diagnostics job.
- [ ] `BUILD_INFO.txt` commit SHA matches the release commit (not an older run or
      a pull request merge commit that differs from the release commit).
- [ ] Build/toolchain metadata matches the verified CI versions and FQBN,
      and reports `diagnostics=0`.
- [ ] No UF2 or generated build metadata is committed to source control.
- [ ] If renamed to `DeskBuddy-v1.0.0.uf2` for release, only the name changes;
      the bytes remain identical to the verified CI production artifact.

## Documentation

- [ ] README matches the hardware and pin assignments.
- [ ] README matches dependency versions.
- [ ] CHANGELOG v1.0.0 is final: replace the preparing/unreleased heading with
      `[1.0.0] - YYYY-MM-DD` using the actual finalized release date, and confirm
      the merged artifact workflow has passed.

## Release

Perform these actions only in the later release pass after all gates above pass.

- [ ] Create an annotated `v1.0.0` tag at the verified release commit.
- [ ] Create the GitHub Release for that tag.
- [ ] Attach the verified production UF2.
- [ ] Publish release notes matching the finalized changelog.

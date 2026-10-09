# Personality V2 release checklist

Complete this checklist for the exact commit intended for **v2.0.0**. Physical
observations and validation must identify the actual flashed commit and hardware.
This checklist does not create a tag, publish a release, or claim completed
physical validation.

## Source

- [ ] `main` is current and all intended release hardening/documentation is merged.
- [ ] No unresolved V2 regressions or temporary debug changes remain.
- [ ] `DESK_BUDDY_DIAGNOSTICS` defaults to `0`; production compiles with `0`.
- [ ] No test-only macros are enabled in the production build.
- [ ] The exact release commit is recorded for tests, builds, device checks, and packaging.

## Tests and builds

- [ ] `./tests/run_host_tests.sh` and `git diff --check` pass.
- [ ] GitHub Actions host tests pass, including the release lifecycle and the
      mandatory multi-minute Buddy-life/invariant/rollover simulations.
- [ ] Production and diagnostics Pico builds pass for `rp2040:rp2040:rpipico`.
- [ ] CI verifies Arduino-Pico 4.3.1, Adafruit GFX 1.12.6, Adafruit ST7735/ST7789
      1.11.0, and Adafruit BusIO 1.17.4; Arduino CLI is 1.3.1.

## Physical validation of the exact candidate

Use [docs/V2_PERSONALITY_TUNING.md](docs/V2_PERSONALITY_TUNING.md) to record results.

- [ ] Boot/display work on the documented Pico/ST7789 hardware.
- [ ] Natural interactions reach Calm/Engaged/Grumpy/Sleepy appropriately;
      decay and affectionate Hold recovery match the observed candidate behavior.
- [ ] Tap and intentional 700 ms Hold are distinct; Hold release adds no Tap.
- [ ] Mood/context-aware Tap and accepted Sound reactions behave as expected.
      Allow audio, reactions, cooldown, and settling to finish between sensor trials.
- [ ] Recent Tap/Hold/Sound sequences and context expiry are checked physically.
- [ ] Production autonomy shows mood-appropriate variety and scheduling; all eight
      autonomous expressions can be inspected in a separate diagnostics build.
- [ ] Entry/exit/replacement and micro-animation show no display corruption,
      Normal flash, visible pupil teleport, or unintended lid/radius snap.
- [ ] ShortPress sleep/wake and 1000 ms Awake LongPress Quiet toggle work;
      Sleeping LongPress wakes without toggling mode.
- [ ] Quiet suppresses production chirps while preserving visuals/system cues,
      survives sleep/wake, and resets to Normal after reboot.
- [ ] Sleep/Wake presentation and cues work; scores survive sleep without
      catch-up decay, and sound alone does not wake Buddy.
- [ ] Buzzer audio does not self-trigger VKLSVAN after settling finishes.
- [ ] Real-device results for this exact candidate are reviewed before any public
      claim that V2 was physically validated/tuned.

## Release artifacts

- [ ] A successful production job exposes `desk-buddy-production-firmware`.
- [ ] Download and inspect exactly one nonempty `DeskBuddy-production.uf2` and
      its generated `BUILD_INFO.txt`.
- [ ] UF2 is from the diagnostics-OFF production build, never the diagnostics job.
- [ ] `BUILD_INFO.txt` commit SHA matches the exact release commit, not an older
      artifact or a differing synthetic PR merge commit.
- [ ] Metadata matches the pinned versions/FQBN and reports `diagnostics=0`.
- [ ] Rename a copy to `DeskBuddy-v2.0.0.uf2` without changing its bytes.
- [ ] Generate `DeskBuddy-v2.0.0.uf2.sha256` from that exact renamed UF2; verify
      the sidecar with `sha256sum -c DeskBuddy-v2.0.0.uf2.sha256`.
- [ ] Retain `BUILD_INFO.txt` alongside the versioned UF2 and checksum.
- [ ] No generated UF2/checksum/build metadata is committed to source control.

## Documentation

- [ ] README describes current V2 behavior, hardware, pin map, diagnostic commands,
      installation, and pinned dependencies.
- [ ] README physical-validation wording matches recorded exact-candidate results.
- [ ] `CHANGELOG.md` `[2.0.0] — Unreleased` becomes `[2.0.0] — YYYY-MM-DD`
      with the actual finalized release date during the final release pass.
- [ ] `RELEASE_NOTES_v2.0.0.md` is the curated user-facing release description.
- [ ] Final release wording identifies published assets accurately; remove
      preparation-only wording once assets are ready for publication.
- [ ] No formal v1.0.0 release history is fabricated.

## Tag and publish — later release pass only

Perform these actions only after every gate above passes and release creation
is requested.

- [ ] Create annotated `v2.0.0` at the verified release commit.
- [ ] Create its GitHub Release using the curated release notes.
- [ ] Attach `DeskBuddy-v2.0.0.uf2`, `DeskBuddy-v2.0.0.uf2.sha256`, and `BUILD_INFO.txt`.
- [ ] Confirm the published files match the verified production artifact and checksum.

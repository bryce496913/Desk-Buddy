#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/desk-buddy-host-tests.XXXXXX")"
trap 'rm -rf "${BUILD_DIR}"' EXIT

readonly -a COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -pedantic
  -I"${REPO_ROOT}/tests"
  -I"${REPO_ROOT}"
)

run_test() {
  local name="$1"
  local output="$2"
  shift 2

  printf '[TEST] %s\n' "${name}"
  g++ "${COMMON_FLAGS[@]}" "$@" -o "${BUILD_DIR}/${output}"
  "${BUILD_DIR}/${output}"
  printf '[PASS] %s\n' "${name}"
}

run_test "production behavior" production_behavior \
  -DDESK_BUDDY_TEST_AUTONOMOUS_SELECTION=1 \
  "${REPO_ROOT}/tests/production_behavior_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "inputs touch gestures" inputs_touch_gestures \
  "${REPO_ROOT}/tests/inputs_touch_gesture_test.cpp" \
  "${REPO_ROOT}/Inputs.cpp"

run_test "SoundSensor" sound_sensor \
  "${REPO_ROOT}/tests/sound_sensor_test.cpp"

run_test "FaceRenderer rollover timing" face_renderer_timing \
  "${REPO_ROOT}/tests/face_renderer_timing_test.cpp"

run_test "FaceRenderer autonomous expressions" face_renderer_expressions \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_expression_test.cpp"

run_test "FaceRenderer interpolation" face_renderer_interpolation \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_interpolation_test.cpp"

run_test "FaceRenderer reaction entry" face_renderer_entry \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_entry_test.cpp"

run_test "FaceRenderer reaction exit" face_renderer_exit \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_exit_test.cpp"

run_test "FaceRenderer micro-animation foundation" face_renderer_micro_animation \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_micro_animation_test.cpp"

run_test "FaceRenderer expression micro-motion" face_renderer_expression_motion \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_expression_motion_test.cpp"

run_test "FaceRenderer micro-animation lifecycle regression" face_renderer_regression \
  -DDESK_BUDDY_DIAGNOSTICS=1 -DDESK_BUDDY_TEST_DRAW_TRACE=1 \
  -I"${REPO_ROOT}/tests/renderer_stubs" \
  "${REPO_ROOT}/tests/face_renderer_regression_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp" "${REPO_ROOT}/Diagnostics.cpp"

run_test "diagnostic BehaviorEngine" diagnostics_behavior \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/diagnostics_behavior_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "BuddyMood state" mood_state \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/mood_state_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "BuddyMood lifecycle" mood_lifecycle \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  -DDESK_BUDDY_TEST_AUTONOMOUS_SELECTION=1 \
  "${REPO_ROOT}/tests/mood_lifecycle_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "weighted autonomous selection" autonomous_weighted_selection \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/autonomous_weighted_selection_test.cpp"

run_test "production autonomous timing" autonomous_timing \
  "${REPO_ROOT}/tests/autonomous_timing_test.cpp"

run_test "diagnostic autonomous scheduling suppression" autonomous_timing_diagnostics \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/autonomous_timing_test.cpp"

run_test "cross interaction" cross_interaction \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/cross_interaction_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp" \
  "${REPO_ROOT}/Diagnostics.cpp"

run_test "production weighted sound variants" sound_variant_selection \
  "${REPO_ROOT}/tests/sound_variant_selection_test.cpp"

run_test "diagnostic weighted sound variants" sound_variant_selection_diagnostics \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/sound_variant_selection_test.cpp"

run_test "SoundEngine diagnostics integration and playback" sound_diagnostics_integration \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/sound_diagnostics_integration_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp" "${REPO_ROOT}/Diagnostics.cpp"

run_test "Quiet audio policy integration" quiet_audio_policy \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/quiet_audio_policy_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp" "${REPO_ROOT}/Diagnostics.cpp"

# Link Diagnostics.cpp separately so missing direct includes fail as they do in Arduino.
run_test "diagnostic Serial parser" diagnostics_parser \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/diagnostics_parser_test.cpp" \
  "${REPO_ROOT}/Diagnostics.cpp"

run_test "diagnostic SoundEngine forced variants" sound_diagnostic_variants \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/sound_diagnostic_variants_test.cpp"

printf '[PASS] all host tests\n'

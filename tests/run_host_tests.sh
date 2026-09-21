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
  "${REPO_ROOT}/tests/production_behavior_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "SoundSensor" sound_sensor \
  "${REPO_ROOT}/tests/sound_sensor_test.cpp"

run_test "FaceRenderer rollover timing" face_renderer_timing \
  "${REPO_ROOT}/tests/face_renderer_timing_test.cpp"

run_test "diagnostic BehaviorEngine" diagnostics_behavior \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/diagnostics_behavior_test.cpp" \
  "${REPO_ROOT}/BehaviorEngine.cpp"

run_test "diagnostic Serial parser" diagnostics_parser \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/diagnostics_parser_test.cpp"

run_test "diagnostic SoundEngine forced variants" sound_diagnostic_variants \
  -DDESK_BUDDY_DIAGNOSTICS=1 \
  "${REPO_ROOT}/tests/sound_diagnostic_variants_test.cpp"

printf '[PASS] all host tests\n'

#include <cassert>
#include <cstdint>
#include <limits>

namespace {
bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void verifyDeadlineAcrossRollover(uint32_t start, uint32_t delay) {
  const uint32_t deadline = start + delay;

  assert(!timeReached(start, deadline));
  assert(!timeReached(start + delay - 1, deadline));
  assert(timeReached(start + delay, deadline));
  assert(timeReached(start + delay + 1, deadline));
}
}  // namespace

int main() {
  constexpr uint32_t nearRollover =
      std::numeric_limits<uint32_t>::max() - 1000;

  // Reaction completion.
  verifyDeadlineAcrossRollover(nearRollover, 1800);

  // Blink, pupil-look, and drowsy-start deadlines.
  verifyDeadlineAcrossRollover(nearRollover, 2000);
  verifyDeadlineAcrossRollover(nearRollover, 1200);
  verifyDeadlineAcrossRollover(nearRollover, 7000);

  // Drowsiness remains active until its deadline, including after rollover.
  const uint32_t drowsyUntil = nearRollover + 1300;
  assert(!timeReached(nearRollover, drowsyUntil));
  assert(!timeReached(drowsyUntil - 1, drowsyUntil));
  assert(timeReached(drowsyUntil, drowsyUntil));

  return 0;
}

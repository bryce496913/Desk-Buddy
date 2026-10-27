#include <cassert>

#include "Settings.h"
#include "EEPROM.h"

int main() {
  // Erased flash (all 0xFF) and blank flash both default to Normal.
  for (uint8_t fill : {uint8_t{0x00}, uint8_t{0xFF}}) {
    for (uint8_t& byte : EEPROM.data) byte = fill;
    beginSettings();
    assert(EEPROM.begun > 0 && loadSoundMode() == BuddySoundMode::Normal);
  }
  // Re-saving the stored mode never touches flash.
  const int commits = EEPROM.commits;
  saveSoundModeIfChanged(BuddySoundMode::Normal);
  assert(EEPROM.commits == commits);

  // Quiet survives a simulated power cycle.
  saveSoundModeIfChanged(BuddySoundMode::Quiet);
  assert(EEPROM.commits == commits + 1);
  saveSoundModeIfChanged(BuddySoundMode::Quiet);
  assert(EEPROM.commits == commits + 1);
  beginSettings();
  assert(loadSoundMode() == BuddySoundMode::Quiet);

  // Returning to Normal is also remembered.
  saveSoundModeIfChanged(BuddySoundMode::Normal);
  assert(EEPROM.commits == commits + 2);
  beginSettings();
  assert(loadSoundMode() == BuddySoundMode::Normal);

  // Unknown stored values fall back to Normal.
  EEPROM.data[1] = 7;
  beginSettings();
  assert(loadSoundMode() == BuddySoundMode::Normal);
}

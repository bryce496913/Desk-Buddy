#include "Settings.h"

#include <EEPROM.h>

namespace {
constexpr size_t SETTINGS_SIZE = 16;
constexpr int MAGIC_ADDR = 0;
constexpr int SOUND_MODE_ADDR = 1;
// Distinguishes saved settings from erased or foreign flash contents.
constexpr uint8_t SETTINGS_MAGIC = 0xB5;
BuddySoundMode storedSoundMode = BuddySoundMode::Normal;
}  // namespace

void beginSettings() {
  EEPROM.begin(SETTINGS_SIZE);
  const bool valid = EEPROM.read(MAGIC_ADDR) == SETTINGS_MAGIC;
  storedSoundMode = valid && EEPROM.read(SOUND_MODE_ADDR) ==
                                 static_cast<uint8_t>(BuddySoundMode::Quiet)
      ? BuddySoundMode::Quiet : BuddySoundMode::Normal;
}

BuddySoundMode loadSoundMode() { return storedSoundMode; }

void saveSoundModeIfChanged(BuddySoundMode mode) {
  if (mode == storedSoundMode) return;
  storedSoundMode = mode;
  EEPROM.write(MAGIC_ADDR, SETTINGS_MAGIC);
  EEPROM.write(SOUND_MODE_ADDR, static_cast<uint8_t>(mode));
  EEPROM.commit();
}

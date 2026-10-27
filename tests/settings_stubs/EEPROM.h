#pragma once
#include <cstddef>
#include <cstdint>
// In-memory stand-in for arduino-pico's flash-backed EEPROM. The array plays
// the role of flash, so calling beginSettings() again simulates a power cycle.
class EEPROMClass {
 public:
  void begin(size_t size) { begun = size; }
  uint8_t read(int address) const { return data[address]; }
  void write(int address, uint8_t value) { data[address] = value; }
  bool commit() { ++commits; return true; }
  uint8_t data[64]{};
  size_t begun = 0;
  int commits = 0;
};
inline EEPROMClass EEPROM;

#pragma once

#include <cstdint>
#include <deque>
#include <sstream>
#include <string>

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t OUTPUT = 1;
constexpr uint8_t INPUT = 0;
constexpr int FALLING = 2;

void pinMode(uint8_t pin, uint8_t mode);
int digitalPinToInterrupt(uint8_t pin);
void attachInterrupt(int interrupt, void (*callback)(), int mode);
void noInterrupts();
void interrupts();
void noTone(uint8_t pin);
void tone(uint8_t pin, unsigned int frequency);
uint32_t millis();
long random(long maximum);
long random(long minimum, long maximum);

class HardwareSerial {
 public:
  int available() const { return static_cast<int>(input.size()); }
  int read() {
    const char value = input.front();
    input.pop_front();
    return value;
  }
  void push(char value) { input.push_back(value); }
  void clearOutput() { output.clear(); }

  void print(const char *value) { output += value; }
  void println() { output += '\n'; }
  void println(const char *value) {
    output += value;
    output += '\n';
  }
  template <typename T>
  void println(T value) {
    std::ostringstream stream;
    stream << value;
    output += stream.str();
    output += '\n';
  }

  std::deque<char> input;
  std::string output;
};

extern HardwareSerial Serial;

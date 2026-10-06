#pragma once
struct SPIStub {
  void setSCK(int) {}
  void setTX(int) {}
  void begin() {}
};
inline SPIStub SPI;

#pragma once

#include <Arduino.h>

constexpr uint8_t TFT_CS = 17;
constexpr uint8_t TFT_DC = 20;
constexpr uint8_t TFT_RST = 21;
constexpr uint8_t TFT_BL = 22;
constexpr uint8_t TFT_SCLK = 18;
constexpr uint8_t TFT_MOSI = 19;

constexpr uint8_t TOUCH_PIN = 5;
constexpr uint8_t BUTTON_PIN = 7;
constexpr uint8_t BUZZER_PIN = 15;
constexpr uint8_t SOUND_SENSOR_PIN = 26;
constexpr uint8_t SOUND_SENSOR_ACTIVE_LEVEL = LOW;
static_assert(SOUND_SENSOR_ACTIVE_LEVEL == LOW, "VKLSVAN trigger must be active LOW");

constexpr int SCREEN_W = 280;
constexpr int SCREEN_H = 240;
constexpr int EYE_REGION_W = 80;
constexpr int EYE_REGION_H = 117;
constexpr int EYE_REGION_Y = 56;
constexpr int EFFECT_REGION_W = 12;
constexpr int EFFECT_REGION_H = 22;

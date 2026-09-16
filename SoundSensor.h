#pragma once

#include <Arduino.h>

void beginSoundSensor();
bool updateSoundSensor(uint32_t now, bool sleeping, bool reactionActive,
                       bool buddyAudioActive);
void finishSoundSensorStartup();
void ignoreSoundSensorAfterWake();

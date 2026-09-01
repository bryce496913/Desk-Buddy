#pragma once

#include <Arduino.h>
#include "BehaviorEngine.h"

void beginFaceRenderer();
void scheduleFaceBehavior(uint32_t now);
void startFaceReaction(uint32_t now);
bool isFaceReactionFinished(uint32_t now);
void finishFaceReaction(uint32_t now);
void enterSleepFace(uint32_t now);
void wakeFace(uint32_t now);
void updateFaceRenderer(uint32_t now, BuddyCoreState coreState, BuddyReaction reaction);

#pragma once
#include "BehaviorEngine.h"
#include "FaceRenderer.h"

inline bool isAutonomousExpressionForMood(FaceExpression expression, BuddyMood mood) {
  switch (mood) {
    case BuddyMood::Calm:
      return expression == FaceExpression::Daydreaming || expression == FaceExpression::SideGlance ||
             expression == FaceExpression::Curious || expression == FaceExpression::Bored;
    case BuddyMood::Engaged:
      return expression == FaceExpression::ExcitedScanning || expression == FaceExpression::Curious ||
             expression == FaceExpression::SideGlance || expression == FaceExpression::Daydreaming;
    case BuddyMood::Grumpy:
      return expression == FaceExpression::AnnoyedSquint || expression == FaceExpression::SuspiciousGlance ||
             expression == FaceExpression::SideGlance || expression == FaceExpression::Bored;
    case BuddyMood::Sleepy:
      return expression == FaceExpression::SleepyDrift || expression == FaceExpression::Daydreaming ||
             expression == FaceExpression::Bored || expression == FaceExpression::SideGlance;
  }
  return false;
}

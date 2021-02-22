#include "MicroModelSettings.h"


PowerLevel kCategoryLabels[kCategoryCount] = {
  PowerLevel::NONE,
  PowerLevel::LOW,
  PowerLevel::HIGH,
  PowerLevel::SILENCE,
  PowerLevel::UNKNOWN,
};

const char* kCategoryTexts[kCategoryCount] = {
  "none",
  "low",
  "high",
  "silence",
  "unknown",
};

#pragma once

#include <cstdint>

namespace rsvp {

enum class DisplayProfile : uint8_t { X3, X4, X4Pro, Uncalibrated };

inline constexpr uint8_t MINIMUM_PACE_WPM = 60;
inline constexpr uint8_t PACE_STEP_WPM = 10;
inline constexpr uint8_t DEFAULT_PACE_WPM = 100;

// Provisional stock-driver budgets, not hardware-qualified minima. See
// docs/rsvp-display-limits.md for panel variants and calibration requirements.
constexpr uint16_t refreshBudgetMs(const DisplayProfile profile) {
  switch (profile) {
    case DisplayProfile::X3:
      return 435;
    case DisplayProfile::X4:
    case DisplayProfile::X4Pro:
      return 500;
    case DisplayProfile::Uncalibrated:
      return 600;
  }
  return 600;
}

constexpr uint8_t maximumPaceWpm(const DisplayProfile profile) {
  return static_cast<uint8_t>((60000u / refreshBudgetMs(profile) / PACE_STEP_WPM) * PACE_STEP_WPM);
}

constexpr uint8_t normalizePaceWpm(const uint32_t pace, const DisplayProfile profile) {
  const uint8_t maximum = maximumPaceWpm(profile);
  if (pace < MINIMUM_PACE_WPM) return MINIMUM_PACE_WPM;
  if (pace > maximum) return maximum;
  return static_cast<uint8_t>(MINIMUM_PACE_WPM + ((pace - MINIMUM_PACE_WPM) / PACE_STEP_WPM) * PACE_STEP_WPM);
}

}  // namespace rsvp

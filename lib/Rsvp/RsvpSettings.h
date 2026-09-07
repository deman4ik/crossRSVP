#pragma once

#include <ArduinoJson.h>

#include <cstdint>

namespace rsvp {

inline constexpr uint8_t SHORT_WORD_GROUPING_DEFAULT = 0;
inline constexpr char SHORT_WORD_GROUPING_SETTING_KEY[] = "rsvpShortWordGrouping";

uint8_t loadShortWordGroupingSetting(JsonVariantConst document);
void saveShortWordGroupingSetting(JsonDocument& document, uint8_t enabled);

}  // namespace rsvp

#pragma once

#include <ArduinoJson.h>

#include <cstdint>

namespace rsvp {

inline constexpr uint8_t CONTEXT_LINE_DEFAULT = 0;
inline constexpr char CONTEXT_LINE_SETTING_KEY[] = "rsvpContextLine";

uint8_t loadContextLineSetting(JsonVariantConst document);
void saveContextLineSetting(JsonDocument& document, uint8_t enabled);

}  // namespace rsvp

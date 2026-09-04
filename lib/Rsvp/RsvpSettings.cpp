#include "RsvpSettings.h"

namespace rsvp {

uint8_t loadContextLineSetting(const JsonVariantConst document) {
  const JsonVariantConst value = document[CONTEXT_LINE_SETTING_KEY];
  if (value.is<bool>()) return value.as<bool>() ? 1 : 0;
  if (value.is<uint8_t>()) {
    const uint8_t numeric = value.as<uint8_t>();
    return numeric <= 1 ? numeric : CONTEXT_LINE_DEFAULT;
  }
  return CONTEXT_LINE_DEFAULT;
}

void saveContextLineSetting(JsonDocument& document, const uint8_t enabled) {
  document[CONTEXT_LINE_SETTING_KEY] = enabled != 0 ? 1 : 0;
}

}  // namespace rsvp

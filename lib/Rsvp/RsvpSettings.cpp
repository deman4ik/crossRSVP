#include "RsvpSettings.h"

namespace rsvp {

uint8_t loadShortWordGroupingSetting(const JsonVariantConst document) {
  const JsonVariantConst value = document[SHORT_WORD_GROUPING_SETTING_KEY];
  if (value.is<bool>()) return value.as<bool>() ? 1 : 0;
  if (value.is<uint8_t>()) {
    const uint8_t numeric = value.as<uint8_t>();
    return numeric <= 1 ? numeric : SHORT_WORD_GROUPING_DEFAULT;
  }
  return SHORT_WORD_GROUPING_DEFAULT;
}

void saveShortWordGroupingSetting(JsonDocument& document, const uint8_t enabled) {
  document[SHORT_WORD_GROUPING_SETTING_KEY] = enabled != 0 ? 1 : 0;
}

}  // namespace rsvp

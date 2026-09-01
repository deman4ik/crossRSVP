#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace VisibleTextUtils {

inline bool equalsTag(const std::string_view name, const std::string_view tag) {
  if (name.size() != tag.size()) return false;
  for (std::size_t i = 0; i < name.size(); i++) {
    const char c = name[i] >= 'A' && name[i] <= 'Z' ? static_cast<char>(name[i] + ('a' - 'A')) : name[i];
    if (c != tag[i]) return false;
  }
  return true;
}

inline bool isNonVisibleElement(const std::string_view name) {
  return equalsTag(name, "head") || equalsTag(name, "style") || equalsTag(name, "script") || equalsTag(name, "title") ||
         equalsTag(name, "rp");
}

inline bool isUtf8Continuation(const uint8_t byte) { return (byte & 0xC0) == 0x80; }

inline bool isNormalizedCrLfContinuation(const uint8_t byte, const bool previousWasCr) {
  return byte == '\n' && previousWasCr;
}

}  // namespace VisibleTextUtils

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BidiUtils {
inline constexpr int RTL_PARAGRAPH_PROBE_DEPTH = 5;
enum class BidiBaseDir : signed char { AUTO = -1, LTR = 0, RTL = 1 };
inline int detectParagraphLevel(const char*, int fallbackLevel = 0, int = 64) { return fallbackLevel; }
inline bool startsWithRtl(const char*, int = RTL_PARAGRAPH_PROBE_DEPTH) { return false; }
inline bool computeVisualWordOrder(const std::vector<std::string>&, bool, std::vector<uint16_t>&) { return false; }
}  // namespace BidiUtils

#pragma once

#include <cstdint>

#include <deque>
#include <string>
#include <vector>

#include "BidiUtils.h"
#include "EpdFontFamily.h"

class GfxRenderer {
 public:
  bool isFontCacheScanning() const { return false; }
  int getFontAscenderSize(int) const { return 0; }
  int getSpaceWidth(int, EpdFontFamily::Style) const { return 0; }
  int getSpaceAdvance(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }
  int getTextAdvanceX(int, const char*, EpdFontFamily::Style) const { return 0; }
  int getTextWidth(int, const char*, EpdFontFamily::Style, BidiUtils::BidiBaseDir) const { return 0; }
  int getKerning(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }
  void drawText(int, int, int, const char*, bool, EpdFontFamily::Style, BidiUtils::BidiBaseDir) const {}
  void drawLine(int, int, int, int, int, bool) const {}
  bool isSdCardFont(int) const { return false; }
  void ensureSdCardFontReady(int, const std::deque<std::string>&, bool, uint8_t = 0x0F) const {}
};

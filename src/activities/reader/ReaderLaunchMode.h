#pragma once

#include <RsvpTypes.h>

#include <cstdint>

enum class ReaderLaunchMode : uint8_t { Paged, Rsvp };

struct ReaderLaunchContext {
  ReaderLaunchMode mode = ReaderLaunchMode::Paged;
  rsvp::ResumeAnchor anchor;
  bool temporaryHighlight = false;
  uint32_t tokenHash32 = 0;
  uint16_t tokenLength = 0;

  ReaderLaunchContext() = default;
  explicit ReaderLaunchContext(ReaderLaunchMode mode) : mode(mode) {}
  ReaderLaunchContext(ReaderLaunchMode mode, rsvp::ResumeAnchor anchor, bool temporaryHighlight = false,
                      uint32_t tokenHash32 = 0, uint16_t tokenLength = 0)
      : mode(mode),
        anchor(anchor),
        temporaryHighlight(temporaryHighlight),
        tokenHash32(tokenHash32),
        tokenLength(tokenLength) {}
};

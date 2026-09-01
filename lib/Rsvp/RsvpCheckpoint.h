#pragma once

#include <cstddef>
#include <cstdint>

#include "RsvpTypes.h"

namespace rsvp {

enum class CheckpointStatus : uint8_t {
  Ok,
  Missing,
  ReadError,
  Corrupt,
  UnsupportedVersion,
  RevisionMismatch,
  Truncated,
  TrailingData,
};

struct RsvpCheckpoint {
  uint64_t bookRevision = 0;
  ResumeAnchor anchor;
  uint32_t tokenHash32 = 0;
  uint16_t tokenLength = 0;
  uint64_t activeRsvpTimeMs = 0;
};

class RsvpCheckpointCodec final {
 public:
  static constexpr uint16_t kFormatVersion = 1;
  static constexpr size_t kEncodedSize = 48;

  // Encodes one fixed-size checkpoint without allocating. The destination
  // remains untouched when it is too small or the anchor is invalid.
  static bool encode(const RsvpCheckpoint& checkpoint, uint8_t* output, size_t outputSize);

  // Decodes exactly one fixed-size checkpoint and validates its CRC and book
  // revision. The output remains untouched unless the result is Ok.
  static CheckpointStatus decode(const uint8_t* data, size_t dataSize, uint64_t expectedBookRevision,
                                 RsvpCheckpoint& output);
};

}  // namespace rsvp

#pragma once

#include <RsvpCheckpoint.h>

#include <cstdint>
#include <string>

namespace rsvp {

class RsvpCheckpointFile final {
 public:
  static bool computeBookRevision(const std::string& bookPath, uint64_t& revision);
  static CheckpointStatus load(const std::string& cachePath, uint64_t expectedRevision, RsvpCheckpoint& checkpoint);
  static bool save(const std::string& cachePath, const RsvpCheckpoint& checkpoint);
  static void invalidate(const std::string& cachePath);
};

}  // namespace rsvp

#pragma once

#include <cstddef>
#include <cstdint>

namespace rsvp {

enum class RefreshKind : uint8_t { Fast, Cleanup };

struct RefreshDistribution {
  static constexpr size_t BUCKET_COUNT = 6;

  uint32_t count = 0;
  uint32_t minimumMs = 0;
  uint32_t maximumMs = 0;
  uint64_t totalMs = 0;
  uint32_t buckets[BUCKET_COUNT] = {};

  uint32_t averageMs() const { return count == 0 ? 0 : static_cast<uint32_t>(totalMs / count); }
};

class RsvpRefreshStats final {
 public:
  void record(RefreshKind kind, uint32_t durationMs);
  const RefreshDistribution& distribution(RefreshKind kind) const;

 private:
  RefreshDistribution fast;
  RefreshDistribution cleanup;
};

}  // namespace rsvp

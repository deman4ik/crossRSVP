#include "RsvpRefreshStats.h"

namespace rsvp {

namespace {

size_t bucketFor(const uint32_t durationMs) {
  constexpr uint32_t upperBounds[] = {250, 500, 750, 1000, 2000};
  for (size_t index = 0; index < sizeof(upperBounds) / sizeof(upperBounds[0]); index++) {
    if (durationMs <= upperBounds[index]) return index;
  }
  return RefreshDistribution::BUCKET_COUNT - 1;
}

}  // namespace

void RsvpRefreshStats::record(const RefreshKind kind, const uint32_t durationMs) {
  auto& value = kind == RefreshKind::Fast ? fast : cleanup;
  if (value.count == 0 || durationMs < value.minimumMs) value.minimumMs = durationMs;
  if (value.count == 0 || durationMs > value.maximumMs) value.maximumMs = durationMs;
  value.count++;
  value.totalMs += durationMs;
  value.buckets[bucketFor(durationMs)]++;
}

const RefreshDistribution& RsvpRefreshStats::distribution(const RefreshKind kind) const {
  return kind == RefreshKind::Fast ? fast : cleanup;
}

}  // namespace rsvp

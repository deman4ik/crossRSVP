#pragma once

#include <cstdint>

namespace rsvp {

// Bounded, allocation-free measurement of the cadence observed by RSVP.
// A run starts with the first accepted frame after construction, interrupt(),
// or a pace change. That frame establishes the baseline and contributes no
// interval; subsequent accepted frames contribute one interval each.
class RsvpSpeedProbe final {
 public:
  static constexpr uint16_t MAXIMUM_WPM = 600;
  static constexpr uint16_t MINIMUM_WPM = 50;
  static constexpr uint16_t STEP_WPM = 25;
  static constexpr uint16_t DEFAULT_WPM = 100;

  bool record(uint32_t frameId, uint16_t paceWpm, uint32_t presentedAtMs, uint32_t refreshMs, bool cleanup) {
    if (frameId == 0 || (hasFrame && frameId == lastFrameId)) return false;

    if (!hasFrame || interrupted || paceWpm != lastPaceWpm) {
      beginRun();
      interrupted = false;
      recordRefresh(refreshMs, cleanup);
      ++acceptedCount;
      hasFrame = true;
      lastFrameId = frameId;
      lastPaceWpm = paceWpm;
      lastPresentedAtMs = presentedAtMs;
      return true;
    }

    const uint32_t intervalMs = presentedAtMs - lastPresentedAtMs;
    lastIntervalMs = intervalMs;
    totalIntervalMs += intervalMs;
    ++intervalSamples;
    ++acceptedCount;
    recordRefresh(refreshMs, cleanup);
    lastFrameId = frameId;
    lastPresentedAtMs = presentedAtMs;
    return true;
  }

  // Stop timing the current run, while retaining its values for display.
  void interrupt() { interrupted = true; }

  uint32_t run() const { return runNumber; }
  uint32_t intervalMs() const { return lastIntervalMs; }
  uint32_t actualFramesPerMinute() const {
    return totalIntervalMs == 0 ? 0 : static_cast<uint32_t>((60000ull * intervalSamples) / totalIntervalMs);
  }
  uint32_t averageFastMs() const {
    return fastRefreshCount == 0 ? 0 : static_cast<uint32_t>(totalFastRefreshMs / fastRefreshCount);
  }
  uint32_t sampleCount() const { return acceptedCount; }
  uint32_t intervalCount() const { return intervalSamples; }

 private:
  void beginRun() {
    ++runNumber;
    totalIntervalMs = 0;
    intervalSamples = 0;
    lastIntervalMs = 0;
    acceptedCount = 0;
    totalFastRefreshMs = 0;
    fastRefreshCount = 0;
  }

  void recordRefresh(const uint32_t refreshMs, const bool cleanup) {
    if (!cleanup) {
      totalFastRefreshMs += refreshMs;
      ++fastRefreshCount;
    }
  }

  uint32_t runNumber = 0;
  uint32_t lastFrameId = 0;
  uint32_t lastPresentedAtMs = 0;
  uint16_t lastPaceWpm = 0;
  uint32_t intervalSamples = 0;
  uint32_t acceptedCount = 0;
  uint32_t lastIntervalMs = 0;
  uint32_t fastRefreshCount = 0;
  uint64_t totalIntervalMs = 0;
  uint64_t totalFastRefreshMs = 0;
  bool hasFrame = false;
  bool interrupted = false;
};

static_assert(sizeof(RsvpSpeedProbe) < 128);

}  // namespace rsvp

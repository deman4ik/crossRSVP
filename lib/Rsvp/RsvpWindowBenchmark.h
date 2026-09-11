#pragma once

#include <cstdint>
#include <limits>

namespace rsvp {

enum class WindowBenchmarkPhase : uint8_t {
  Idle,
  FullWarmup,
  FullMeasurement,
  WindowWarmup,
  WindowMeasurement,
  Complete,
  Aborted,
  Error,
};

enum class WindowActualRefresh : uint8_t { None, Window, Full, Cleanup };

enum class WindowFallback : uint8_t {
  None,
  CandidateDisabled,
  UnsupportedModel,
  UnsupportedController,
  InconclusiveController,
  Inverted,
  InvalidGeometry,
  InvalidBaseline,
  BusyTimeout,
  ControllerError,
};

struct WindowDurationStats {
  uint32_t count = 0;
  uint32_t minimumMs = 0;
  uint32_t maximumMs = 0;
  uint64_t totalMs = 0;

  void record(const uint32_t durationMs) {
    // Freeze the distribution at capacity so count and total retain the same
    // sample population and the mean cannot become biased after saturation.
    if (count == std::numeric_limits<uint32_t>::max()) return;
    if (count == 0 || durationMs < minimumMs) minimumMs = durationMs;
    if (durationMs > maximumMs) maximumMs = durationMs;
    ++count;
    const uint64_t remaining = std::numeric_limits<uint64_t>::max() - totalMs;
    totalMs += durationMs > remaining ? remaining : durationMs;
  }

  uint32_t meanMs() const {
    if (count == 0) return 0;
    const uint64_t mean = totalMs / count;
    return mean > std::numeric_limits<uint32_t>::max() ? std::numeric_limits<uint32_t>::max()
                                                       : static_cast<uint32_t>(mean);
  }
};

struct WindowBranchStats {
  WindowDurationStats interval;
  WindowDurationStats frame;
  WindowDurationStats refresh;
  uint32_t windowCount = 0;
  uint32_t fullCount = 0;
  uint32_t cleanupCount = 0;
  uint32_t fallbackCount = 0;
  uint32_t failureCount = 0;
  uint16_t minimumCpuMhz = 0;
  uint16_t maximumCpuMhz = 0;
  uint32_t cpuSampleCount = 0;
  WindowFallback lastFallback = WindowFallback::None;
  uint32_t elapsedMs = 0;

  uint32_t framesPerMinute() const {
    return interval.totalMs == 0
               ? 0
               : static_cast<uint32_t>((60000ull * static_cast<uint64_t>(interval.count)) / interval.totalMs);
  }
};

// Fixed-size controller for an autonomous, comparable full/window A/B run.
// Phase transitions are driven only by update(); the activity performs a full
// presentation and restarts its deterministic source after each transition.
class RsvpWindowBenchmark final {
 public:
  static constexpr uint32_t WARMUP_MS = 5000;
  static constexpr uint32_t MEASUREMENT_MS = 60000;
  static constexpr uint16_t PACE_WPM = 300;

  void start(const uint32_t nowMs) {
    full = {};
    window = {};
    phase = WindowBenchmarkPhase::FullWarmup;
    phaseStartedAtMs = nowMs;
    previousPresentedAtMs = 0;
    hasPreviousPresentation = false;
    previousFrameId = 0;
  }

  bool update(const uint32_t nowMs) {
    const uint32_t elapsed = nowMs - phaseStartedAtMs;
    const uint32_t required = isWarmup() ? WARMUP_MS : isMeasurement() ? MEASUREMENT_MS : 0;
    if (required == 0 || elapsed < required) {
      updateElapsed(elapsed);
      return false;
    }

    switch (phase) {
      case WindowBenchmarkPhase::FullWarmup:
        phase = WindowBenchmarkPhase::FullMeasurement;
        break;
      case WindowBenchmarkPhase::FullMeasurement:
        full.elapsedMs = elapsed;
        phase = WindowBenchmarkPhase::WindowWarmup;
        break;
      case WindowBenchmarkPhase::WindowWarmup:
        phase = WindowBenchmarkPhase::WindowMeasurement;
        break;
      case WindowBenchmarkPhase::WindowMeasurement:
        window.elapsedMs = elapsed;
        phase = WindowBenchmarkPhase::Complete;
        break;
      default:
        return false;
    }
    phaseStartedAtMs = nowMs;
    previousPresentedAtMs = 0;
    hasPreviousPresentation = false;
    previousFrameId = 0;
    return true;
  }

  void record(const uint32_t presentedAtMs, const uint32_t frameMs, const uint32_t refreshMs,
              const WindowActualRefresh actual, const WindowFallback fallback, const bool success,
              const uint16_t cpuMhz = 0, const uint32_t frameId = 0) {
    const bool fullBranch = phase == WindowBenchmarkPhase::FullWarmup || phase == WindowBenchmarkPhase::FullMeasurement;
    const bool windowBranch =
        phase == WindowBenchmarkPhase::WindowWarmup || phase == WindowBenchmarkPhase::WindowMeasurement;
    if (!fullBranch && !windowBranch) return;
    WindowBranchStats& stats = fullBranch ? full : window;
    if (!success) {
      increment(stats.failureCount);
      if (fallback != WindowFallback::None) {
        increment(stats.fallbackCount);
        stats.lastFallback = fallback;
      }
      return;
    }
    if (frameId != 0 && frameId == previousFrameId) return;
    previousFrameId = frameId;
    if (!isMeasurement()) return;
    if (cpuMhz != 0 && stats.cpuSampleCount != std::numeric_limits<uint32_t>::max()) {
      if (stats.cpuSampleCount == 0 || cpuMhz < stats.minimumCpuMhz) stats.minimumCpuMhz = cpuMhz;
      if (cpuMhz > stats.maximumCpuMhz) stats.maximumCpuMhz = cpuMhz;
      ++stats.cpuSampleCount;
    }
    stats.frame.record(frameMs);
    stats.refresh.record(refreshMs);
    if (hasPreviousPresentation) stats.interval.record(presentedAtMs - previousPresentedAtMs);
    previousPresentedAtMs = presentedAtMs;
    hasPreviousPresentation = true;
    switch (actual) {
      case WindowActualRefresh::Window:
        increment(stats.windowCount);
        break;
      case WindowActualRefresh::Full:
        increment(stats.fullCount);
        break;
      case WindowActualRefresh::Cleanup:
        increment(stats.cleanupCount);
        break;
      case WindowActualRefresh::None:
        break;
    }
    if (fallback != WindowFallback::None) {
      increment(stats.fallbackCount);
      stats.lastFallback = fallback;
    }
  }

  void abort(const uint32_t nowMs) {
    updateElapsed(nowMs - phaseStartedAtMs);
    phase = WindowBenchmarkPhase::Aborted;
  }

  void fail(const uint32_t nowMs) {
    updateElapsed(nowMs - phaseStartedAtMs);
    phase = WindowBenchmarkPhase::Error;
  }

  WindowBenchmarkPhase currentPhase() const { return phase; }
  bool running() const { return isWarmup() || isMeasurement(); }
  bool windowCandidateEnabled() const {
    return phase == WindowBenchmarkPhase::WindowWarmup || phase == WindowBenchmarkPhase::WindowMeasurement;
  }
  const WindowBranchStats& fullStats() const { return full; }
  const WindowBranchStats& windowStats() const { return window; }

 private:
  static void increment(uint32_t& value) {
    if (value != std::numeric_limits<uint32_t>::max()) ++value;
  }

  bool isWarmup() const {
    return phase == WindowBenchmarkPhase::FullWarmup || phase == WindowBenchmarkPhase::WindowWarmup;
  }
  bool isMeasurement() const {
    return phase == WindowBenchmarkPhase::FullMeasurement || phase == WindowBenchmarkPhase::WindowMeasurement;
  }
  void updateElapsed(const uint32_t elapsed) {
    if (phase == WindowBenchmarkPhase::FullMeasurement) full.elapsedMs = elapsed;
    if (phase == WindowBenchmarkPhase::WindowMeasurement) window.elapsedMs = elapsed;
  }

  WindowBranchStats full;
  WindowBranchStats window;
  WindowBenchmarkPhase phase = WindowBenchmarkPhase::Idle;
  uint32_t phaseStartedAtMs = 0;
  uint32_t previousPresentedAtMs = 0;
  bool hasPreviousPresentation = false;
  uint32_t previousFrameId = 0;
};

static_assert(sizeof(RsvpWindowBenchmark) < 256);

}  // namespace rsvp

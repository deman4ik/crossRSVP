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
  TightProbe,
  TightWarmup,
  TightMeasurement,
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
  BusyNotReady,
  BusyTimeout,
  ControllerError,
};

// The diagnostic keeps the display observations separate from the long-run
// timing counters. These values are deliberately HAL-independent so the
// bounded snapshot can be used by host tests without pulling in the display
// driver.
enum class WindowDiagnosticPhase : uint8_t {
  Ready,
  FullSizePtl,
  LineCandidate,
  FullWarmup,
  FullMeasurement,
  WindowWarmup,
  WindowMeasurement,
  TightProbe,
  TightWarmup,
  TightMeasurement,
  Complete,
  Aborted,
  Error,
};

enum class WindowDiagnosticOperation : uint8_t {
  None,
  FullFrame,
  FullResync,
  FullSizePtl,
  LineCandidate,
  TightProbe,
  TightWindow,
  FullRecovery,
};

enum class WindowDiagnosticError : uint8_t {
  None,
  InvalidRegion,
  BusyNotReady,
  BusyTimeout,
  Other,
};

// A primitive copy of the optional controller trace. Keeping SDK enums out of
// the report model lets host tests and CSV persistence stay independent of the
// patched FreeInk headers while retaining the exact trace captured at failure.
struct WindowDiagnosticTrace {
  bool valid = false;
  uint8_t phase = 0;
  uint8_t stage = 0;
  uint8_t error = 0;
  uint8_t wait = 0;
  bool busyBefore = false;
  bool busyAfter = false;
  uint8_t baselineBefore = 0;
  uint8_t baselineAfter = 0;
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t payloadBytes = 0;
  bool refreshTriggered = false;
};

struct WindowDiagnosticFailure {
  bool valid = false;
  WindowDiagnosticPhase phase = WindowDiagnosticPhase::Ready;
  WindowDiagnosticOperation operation = WindowDiagnosticOperation::None;
  WindowActualRefresh requested = WindowActualRefresh::None;
  WindowActualRefresh actual = WindowActualRefresh::None;
  WindowDiagnosticError error = WindowDiagnosticError::None;
  uint8_t rawError = 0;
  uint32_t durationMs = 0;
  uint16_t warmupSuccesses = 0;
  WindowDiagnosticOperation lastSuccessfulOperation = WindowDiagnosticOperation::None;
  WindowDiagnosticTrace trace;
};

// Fixed-size, allocation-free evidence for one diagnostic run. Recovery
// successes update the operation counters but never erase lastFailure.
struct WindowDiagnosticState {
  WindowDiagnosticOperation lastOperation = WindowDiagnosticOperation::None;
  WindowDiagnosticOperation lastSuccessfulOperation = WindowDiagnosticOperation::None;
  uint16_t fullWarmupSuccesses = 0;
  uint16_t windowWarmupSuccesses = 0;
  uint16_t tightWarmupSuccesses = 0;
  WindowDiagnosticFailure lastFailure;

  void reset() { *this = {}; }

  void recordAttempt(const WindowDiagnosticPhase phase, const WindowDiagnosticOperation operation,
                     const WindowActualRefresh requested, const WindowActualRefresh actual,
                     const WindowDiagnosticError error, const uint8_t rawError, const uint32_t durationMs,
                     const bool success, const WindowDiagnosticTrace& trace = {}) {
    lastOperation = operation;
    if (success) {
      lastSuccessfulOperation = operation;
      if (phase == WindowDiagnosticPhase::FullWarmup) increment(fullWarmupSuccesses);
      if (phase == WindowDiagnosticPhase::WindowWarmup) increment(windowWarmupSuccesses);
      if (phase == WindowDiagnosticPhase::TightWarmup) increment(tightWarmupSuccesses);
      return;
    }

    if (!lastFailure.valid) {
      lastFailure = {.valid = true,
                     .phase = phase,
                     .operation = operation,
                     .requested = requested,
                     .actual = actual,
                     .error = error,
                     .rawError = rawError,
                     .durationMs = durationMs,
                     .warmupSuccesses = warmupSuccessesFor(phase),
                     .lastSuccessfulOperation = lastSuccessfulOperation,
                     .trace = trace};
    }
  }

  uint16_t warmupSuccessesFor(const WindowDiagnosticPhase phase) const {
    if (phase == WindowDiagnosticPhase::FullWarmup) return fullWarmupSuccesses;
    if (phase == WindowDiagnosticPhase::WindowWarmup) return windowWarmupSuccesses;
    if (phase == WindowDiagnosticPhase::TightWarmup) return tightWarmupSuccesses;
    return 0;
  }

  uint16_t warmupSuccessCount() const {
    const uint32_t total = static_cast<uint32_t>(fullWarmupSuccesses) + windowWarmupSuccesses + tightWarmupSuccesses;
    return total > std::numeric_limits<uint16_t>::max() ? std::numeric_limits<uint16_t>::max()
                                                        : static_cast<uint16_t>(total);
  }

 private:
  static void increment(uint16_t& value) {
    if (value != std::numeric_limits<uint16_t>::max()) ++value;
  }
};

static_assert(sizeof(WindowDiagnosticState) <= 80);

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

struct WindowRoiStats {
  uint32_t sampleCount = 0;
  uint16_t minimumWidth = 0;
  uint16_t maximumWidth = 0;
  uint16_t minimumHeight = 0;
  uint16_t maximumHeight = 0;

  void reset() { *this = {}; }

  void record(const uint16_t width, const uint16_t height) {
    if (width == 0 || height == 0 || sampleCount == std::numeric_limits<uint32_t>::max()) return;
    if (sampleCount == 0 || width < minimumWidth) minimumWidth = width;
    if (sampleCount == 0 || width > maximumWidth) maximumWidth = width;
    if (sampleCount == 0 || height < minimumHeight) minimumHeight = height;
    if (sampleCount == 0 || height > maximumHeight) maximumHeight = height;
    ++sampleCount;
  }
};

static_assert(sizeof(WindowRoiStats) <= 16);

struct WindowBranchStats {
  WindowDurationStats interval;
  WindowDurationStats frame;
  WindowDurationStats refresh;
  WindowRoiStats roi;
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
  static constexpr uint16_t UNLIMITED_PACE_WPM = std::numeric_limits<uint16_t>::max();
  static constexpr uint16_t REPORT_TARGET_WPM = 0;
  // Kept for host callers that used the original fixed-rate probe constant.
  // Firmware diagnostics use UNLIMITED_PACE_WPM so sub-200 ms windows are not
  // capped by the historical 300 WPM setting.
  static constexpr uint16_t PACE_WPM = 300;

  void reset() {
    full = {};
    window = {};
    tight = {};
    phase = WindowBenchmarkPhase::Idle;
    phaseStartedAtMs = 0;
    previousPresentedAtMs = 0;
    hasPreviousPresentation = false;
    previousFrameId = 0;
  }

  void start(const uint32_t nowMs) {
    reset();
    phase = WindowBenchmarkPhase::FullWarmup;
    phaseStartedAtMs = nowMs;
    previousPresentedAtMs = 0;
    hasPreviousPresentation = false;
    previousFrameId = 0;
  }

  // Preserve the completed full/window samples while starting the separately
  // gated tight-area run. The activity must show and confirm the tight probe
  // before calling this method.
  void startTight(const uint32_t nowMs) {
    tight = {};
    phase = WindowBenchmarkPhase::TightWarmup;
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
        phase = WindowBenchmarkPhase::TightProbe;
        break;
      case WindowBenchmarkPhase::TightWarmup:
        phase = WindowBenchmarkPhase::TightMeasurement;
        break;
      case WindowBenchmarkPhase::TightMeasurement:
        tight.elapsedMs = elapsed;
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
    const bool tightBranch =
        phase == WindowBenchmarkPhase::TightWarmup || phase == WindowBenchmarkPhase::TightMeasurement;
    if (!fullBranch && !windowBranch && !tightBranch) return;
    WindowBranchStats& stats = fullBranch ? full : windowBranch ? window : tight;
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
  bool tightCandidateEnabled() const {
    return phase == WindowBenchmarkPhase::TightWarmup || phase == WindowBenchmarkPhase::TightMeasurement;
  }
  const WindowBranchStats& fullStats() const { return full; }
  const WindowBranchStats& windowStats() const { return window; }
  const WindowBranchStats& tightStats() const { return tight; }
  const WindowRoiStats& tightRoiStats() const { return tight.roi; }

  void recordRoi(const uint16_t width, const uint16_t height) {
    const bool windowBranch = phase == WindowBenchmarkPhase::WindowMeasurement;
    const bool tightBranch = phase == WindowBenchmarkPhase::TightMeasurement;
    if (windowBranch) window.roi.record(width, height);
    if (tightBranch) tight.roi.record(width, height);
  }

  void recordTightRoi(const uint16_t width, const uint16_t height) { recordRoi(width, height); }

 private:
  static void increment(uint32_t& value) {
    if (value != std::numeric_limits<uint32_t>::max()) ++value;
  }

  bool isWarmup() const {
    return phase == WindowBenchmarkPhase::FullWarmup || phase == WindowBenchmarkPhase::WindowWarmup ||
           phase == WindowBenchmarkPhase::TightWarmup;
  }
  bool isMeasurement() const {
    return phase == WindowBenchmarkPhase::FullMeasurement || phase == WindowBenchmarkPhase::WindowMeasurement ||
           phase == WindowBenchmarkPhase::TightMeasurement;
  }
  void updateElapsed(const uint32_t elapsed) {
    if (phase == WindowBenchmarkPhase::FullMeasurement) full.elapsedMs = elapsed;
    if (phase == WindowBenchmarkPhase::WindowMeasurement) window.elapsedMs = elapsed;
    if (phase == WindowBenchmarkPhase::TightMeasurement) tight.elapsedMs = elapsed;
  }

  WindowBranchStats full;
  WindowBranchStats window;
  WindowBranchStats tight;
  WindowBenchmarkPhase phase = WindowBenchmarkPhase::Idle;
  uint32_t phaseStartedAtMs = 0;
  uint32_t previousPresentedAtMs = 0;
  bool hasPreviousPresentation = false;
  uint32_t previousFrameId = 0;
};

static_assert(sizeof(RsvpWindowBenchmark) < 400);

}  // namespace rsvp

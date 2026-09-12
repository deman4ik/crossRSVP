#pragma once

#include <RsvpWindowBenchmark.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace rsvp {

struct WindowReportMetadata {
  const char* controller = "unknown";
  const char* confidence = "inconclusive";
  const char* orientation = "unknown";
  const char* power = "not_sampled";
  // A zero target records the diagnostic's uncapped pacing mode; the fixture
  // uses RsvpWindowBenchmark::UNLIMITED_PACE_WPM internally.
  uint16_t targetWpm = RsvpWindowBenchmark::REPORT_TARGET_WPM;
  const char* firmwareVersion = "unknown";
  const char* diagnosticStage = "ready";
  const char* diagnosticStatus = "ready";
  bool fullProbeConfirmed = false;
  bool lineProbeConfirmed = false;
  bool tightProbeConfirmed = false;
};

class WindowReportSink {
 public:
  virtual ~WindowReportSink() = default;
  virtual bool write(const char* data, size_t size) = 0;
};

inline const char* windowFallbackName(const WindowFallback fallback) {
  switch (fallback) {
    case WindowFallback::None:
      return "none";
    case WindowFallback::CandidateDisabled:
      return "candidate_disabled";
    case WindowFallback::UnsupportedModel:
      return "unsupported_model";
    case WindowFallback::UnsupportedController:
      return "unsupported_controller";
    case WindowFallback::InconclusiveController:
      return "inconclusive_controller";
    case WindowFallback::Inverted:
      return "inverted";
    case WindowFallback::InvalidGeometry:
      return "invalid_geometry";
    case WindowFallback::InvalidBaseline:
      return "invalid_baseline";
    case WindowFallback::BusyNotReady:
      return "busy_not_ready";
    case WindowFallback::BusyTimeout:
      return "busy_timeout";
    case WindowFallback::ControllerError:
      return "controller_error";
  }
  return "unknown";
}

inline const char* windowActualRefreshName(const WindowActualRefresh actual) {
  switch (actual) {
    case WindowActualRefresh::None:
      return "none";
    case WindowActualRefresh::Window:
      return "window";
    case WindowActualRefresh::Full:
      return "full";
    case WindowActualRefresh::Cleanup:
      return "cleanup";
  }
  return "unknown";
}

inline const char* windowDiagnosticPhaseName(const WindowDiagnosticPhase phase) {
  switch (phase) {
    case WindowDiagnosticPhase::Ready:
      return "ready";
    case WindowDiagnosticPhase::FullSizePtl:
      return "full_size_ptl";
    case WindowDiagnosticPhase::LineCandidate:
      return "line_candidate";
    case WindowDiagnosticPhase::FullWarmup:
      return "full_warmup";
    case WindowDiagnosticPhase::FullMeasurement:
      return "full_measurement";
    case WindowDiagnosticPhase::WindowWarmup:
      return "window_warmup";
    case WindowDiagnosticPhase::WindowMeasurement:
      return "window_measurement";
    case WindowDiagnosticPhase::TightProbe:
      return "tight_probe";
    case WindowDiagnosticPhase::TightWarmup:
      return "tight_warmup";
    case WindowDiagnosticPhase::TightMeasurement:
      return "tight_measurement";
    case WindowDiagnosticPhase::Complete:
      return "complete";
    case WindowDiagnosticPhase::Aborted:
      return "aborted";
    case WindowDiagnosticPhase::Error:
      return "error";
  }
  return "unknown";
}

inline const char* windowDiagnosticOperationName(const WindowDiagnosticOperation operation) {
  switch (operation) {
    case WindowDiagnosticOperation::None:
      return "none";
    case WindowDiagnosticOperation::FullFrame:
      return "full_frame";
    case WindowDiagnosticOperation::FullResync:
      return "full_resync";
    case WindowDiagnosticOperation::FullSizePtl:
      return "full_size_ptl";
    case WindowDiagnosticOperation::LineCandidate:
      return "line_candidate";
    case WindowDiagnosticOperation::TightProbe:
      return "tight_probe";
    case WindowDiagnosticOperation::TightWindow:
      return "tight_window";
    case WindowDiagnosticOperation::FullRecovery:
      return "full_recovery";
  }
  return "unknown";
}

inline const char* windowDiagnosticErrorName(const WindowDiagnosticError error) {
  switch (error) {
    case WindowDiagnosticError::None:
      return "none";
    case WindowDiagnosticError::InvalidRegion:
      return "invalid_region";
    case WindowDiagnosticError::BusyNotReady:
      return "busy_not_ready";
    case WindowDiagnosticError::BusyTimeout:
      return "busy_timeout";
    case WindowDiagnosticError::Other:
      return "other";
  }
  return "unknown";
}

namespace detail {
inline bool writeText(WindowReportSink& sink, const char* text) { return sink.write(text, strlen(text)); }

inline bool writeNumber(WindowReportSink& sink, const uint64_t number) {
  char value[24];
  const int length = snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(number));
  return length > 0 && static_cast<size_t>(length) < sizeof(value) && sink.write(value, static_cast<size_t>(length));
}

inline bool writeDistribution(WindowReportSink& sink, const WindowDurationStats& stats) {
  return writeNumber(sink, stats.count) && writeText(sink, ",") && writeNumber(sink, stats.minimumMs) &&
         writeText(sink, ",") && writeNumber(sink, stats.meanMs()) && writeText(sink, ",") &&
         writeNumber(sink, stats.maximumMs);
}

inline bool writeDiagnosticFields(WindowReportSink& sink, const WindowDiagnosticState& diagnostic) {
  const auto& failure = diagnostic.lastFailure;
  return writeText(sink, windowDiagnosticOperationName(diagnostic.lastOperation)) && writeText(sink, ",") &&
         writeText(sink, windowDiagnosticOperationName(diagnostic.lastSuccessfulOperation)) && writeText(sink, ",") &&
         writeNumber(sink, diagnostic.fullWarmupSuccesses) && writeText(sink, ",") &&
         writeNumber(sink, diagnostic.windowWarmupSuccesses) && writeText(sink, ",") &&
         writeText(sink, failure.valid ? windowDiagnosticPhaseName(failure.phase) : "none") && writeText(sink, ",") &&
         writeText(sink, failure.valid ? windowDiagnosticOperationName(failure.operation) : "none") &&
         writeText(sink, ",") && writeText(sink, failure.valid ? windowActualRefreshName(failure.requested) : "none") &&
         writeText(sink, ",") && writeText(sink, failure.valid ? windowActualRefreshName(failure.actual) : "none") &&
         writeText(sink, ",") && writeText(sink, failure.valid ? windowDiagnosticErrorName(failure.error) : "none") &&
         writeText(sink, ",") && writeNumber(sink, failure.valid ? failure.rawError : 0) && writeText(sink, ",") &&
         writeNumber(sink, failure.valid ? failure.durationMs : 0) && writeText(sink, ",") &&
         writeNumber(sink, failure.valid ? failure.warmupSuccesses : 0) && writeText(sink, ",") &&
         writeText(sink, failure.valid ? windowDiagnosticOperationName(failure.lastSuccessfulOperation) : "none");
}

inline bool writeTraceFields(WindowReportSink& sink, const WindowDiagnosticState& diagnostic) {
  const auto& trace = diagnostic.lastFailure.trace;
  return writeNumber(sink, trace.valid ? 1 : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.phase : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.stage : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.error : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.wait : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid && trace.busyBefore ? 1 : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid && trace.busyAfter ? 1 : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.baselineBefore : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.baselineAfter : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.x : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.y : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.width : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.height : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid ? trace.payloadBytes : 0) && writeText(sink, ",") &&
         writeNumber(sink, trace.valid && trace.refreshTriggered ? 1 : 0);
}

inline bool writeTightFields(WindowReportSink& sink, const WindowReportMetadata& metadata,
                             const WindowBranchStats& stats) {
  const auto& roi = stats.roi;
  return writeNumber(sink, metadata.tightProbeConfirmed ? 1 : 0) && writeText(sink, ",") &&
         writeNumber(sink, roi.sampleCount) && writeText(sink, ",") && writeNumber(sink, roi.minimumWidth) &&
         writeText(sink, ",") && writeNumber(sink, roi.maximumWidth) && writeText(sink, ",") &&
         writeNumber(sink, roi.minimumHeight) && writeText(sink, ",") && writeNumber(sink, roi.maximumHeight);
}

inline bool writeRow(WindowReportSink& sink, const char* variant, const WindowBranchStats& stats,
                     const WindowReportMetadata& metadata, const WindowDiagnosticState& diagnostic) {
  return writeText(sink, variant) && writeText(sink, ",") && writeText(sink, metadata.controller) &&
         writeText(sink, ",") && writeText(sink, metadata.confidence) && writeText(sink, ",") &&
         writeText(sink, metadata.orientation) && writeText(sink, ",") && writeText(sink, metadata.power) &&
         writeText(sink, ",") && writeNumber(sink, metadata.targetWpm) && writeText(sink, ",") &&
         writeNumber(sink, stats.elapsedMs) && writeText(sink, ",") && writeNumber(sink, stats.cpuSampleCount) &&
         writeText(sink, ",") && writeNumber(sink, stats.minimumCpuMhz) && writeText(sink, ",") &&
         writeNumber(sink, stats.maximumCpuMhz) && writeText(sink, ",") && writeDistribution(sink, stats.frame) &&
         writeText(sink, ",") && writeDistribution(sink, stats.interval) && writeText(sink, ",") &&
         writeDistribution(sink, stats.refresh) && writeText(sink, ",") && writeNumber(sink, stats.framesPerMinute()) &&
         writeText(sink, ",") && writeNumber(sink, stats.windowCount) && writeText(sink, ",") &&
         writeNumber(sink, stats.fullCount) && writeText(sink, ",") && writeNumber(sink, stats.cleanupCount) &&
         writeText(sink, ",") && writeNumber(sink, stats.fallbackCount) && writeText(sink, ",") &&
         writeText(sink, windowFallbackName(stats.lastFallback)) && writeText(sink, ",") &&
         writeNumber(sink, stats.failureCount) && writeText(sink, ",") && writeDiagnosticFields(sink, diagnostic) &&
         writeText(sink, ",") && writeText(sink, metadata.firmwareVersion) && writeText(sink, ",") &&
         writeText(sink, metadata.diagnosticStage) && writeText(sink, ",") &&
         writeText(sink, metadata.diagnosticStatus) && writeText(sink, ",") &&
         writeNumber(sink, metadata.fullProbeConfirmed ? 1 : 0) && writeText(sink, ",") &&
         writeNumber(sink, metadata.lineProbeConfirmed ? 1 : 0) && writeText(sink, ",") &&
         writeTraceFields(sink, diagnostic) && writeText(sink, ",") && writeTightFields(sink, metadata, stats) &&
         writeText(sink, "\n");
}
}  // namespace detail

inline bool writeRsvpWindowReport(WindowReportSink& sink, const RsvpWindowBenchmark& benchmark,
                                  const WindowReportMetadata& metadata, const WindowDiagnosticState& diagnostic);

inline bool writeRsvpWindowReport(WindowReportSink& sink, const RsvpWindowBenchmark& benchmark,
                                  const WindowReportMetadata& metadata) {
  return writeRsvpWindowReport(sink, benchmark, metadata, {});
}

inline bool writeRsvpWindowReport(WindowReportSink& sink, const RsvpWindowBenchmark& benchmark,
                                  const WindowReportMetadata& metadata, const WindowDiagnosticState& diagnostic) {
  static constexpr char HEADER[] =
      "variant,controller,confidence,orientation,power,target_wpm,elapsed_ms,"
      "cpu_sample_count,cpu_min_mhz,cpu_max_mhz,"
      "frame_count,frame_min_ms,frame_mean_ms,frame_max_ms,"
      "interval_count,interval_min_ms,interval_mean_ms,interval_max_ms,"
      "refresh_count,refresh_min_ms,refresh_mean_ms,refresh_max_ms,frames_per_minute,"
      "actual_window_count,actual_full_count,actual_cleanup_count,fallback_count,last_fallback,failure_count,"
      "last_operation,last_successful_operation,full_warmup_successes,window_warmup_successes,failure_phase,"
      "failure_operation,"
      "failure_requested,failure_actual,failure_error,failure_error_code,failure_duration_ms,"
      "failure_warmup_successes,failure_last_successful_operation,firmware_version,diagnostic_stage,"
      "diagnostic_status,full_probe_confirmed,line_probe_confirmed,trace_valid,trace_phase,trace_stage,"
      "trace_error,trace_wait,trace_busy_before,trace_busy_after,trace_baseline_before,trace_baseline_after,"
      "trace_x,trace_y,trace_width,trace_height,trace_payload_bytes,trace_refresh_triggered,"
      "tight_probe_confirmed,roi_sample_count,roi_min_width,roi_max_width,roi_min_height,roi_max_height\n";
  return detail::writeText(sink, HEADER) &&
         detail::writeRow(sink, "full", benchmark.fullStats(), metadata, diagnostic) &&
         detail::writeRow(sink, "window", benchmark.windowStats(), metadata, diagnostic) &&
         detail::writeRow(sink, "tight_window", benchmark.tightStats(), metadata, diagnostic);
}

}  // namespace rsvp

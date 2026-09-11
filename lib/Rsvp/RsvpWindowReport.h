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
  uint16_t targetWpm = RsvpWindowBenchmark::PACE_WPM;
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
    case WindowFallback::BusyTimeout:
      return "busy_timeout";
    case WindowFallback::ControllerError:
      return "controller_error";
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

inline bool writeRow(WindowReportSink& sink, const char* variant, const WindowBranchStats& stats,
                     const WindowReportMetadata& metadata) {
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
         writeNumber(sink, stats.failureCount) && writeText(sink, "\n");
}
}  // namespace detail

inline bool writeRsvpWindowReport(WindowReportSink& sink, const RsvpWindowBenchmark& benchmark,
                                  const WindowReportMetadata& metadata) {
  static constexpr char HEADER[] =
      "variant,controller,confidence,orientation,power,target_wpm,elapsed_ms,"
      "cpu_sample_count,cpu_min_mhz,cpu_max_mhz,"
      "frame_count,frame_min_ms,frame_mean_ms,frame_max_ms,"
      "interval_count,interval_min_ms,interval_mean_ms,interval_max_ms,"
      "refresh_count,refresh_min_ms,refresh_mean_ms,refresh_max_ms,frames_per_minute,"
      "actual_window_count,actual_full_count,actual_cleanup_count,fallback_count,last_fallback,failure_count\n";
  return detail::writeText(sink, HEADER) && detail::writeRow(sink, "full", benchmark.fullStats(), metadata) &&
         detail::writeRow(sink, "window", benchmark.windowStats(), metadata);
}

}  // namespace rsvp

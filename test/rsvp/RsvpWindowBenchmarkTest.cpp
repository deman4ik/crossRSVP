#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "RsvpWindowBenchmark.h"
#include "RsvpWindowFixtureSource.h"
#include "RsvpWindowReport.h"

namespace {
class StringSink final : public rsvp::WindowReportSink {
 public:
  bool write(const char* data, const size_t size) override {
    if (failWrites) return false;
    output.append(data, size);
    return true;
  }

  std::string output;
  bool failWrites = false;
};
}  // namespace

TEST(RsvpWindowBenchmark, RunsEqualWarmupAndSixtySecondMeasurementPhases) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(1000);
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::FullWarmup);
  EXPECT_FALSE(benchmark.update(5999));
  EXPECT_TRUE(benchmark.update(6000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::FullMeasurement);
  EXPECT_FALSE(benchmark.update(65999));
  EXPECT_TRUE(benchmark.update(66000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::WindowWarmup);
  EXPECT_TRUE(benchmark.update(71000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::WindowMeasurement);
  EXPECT_FALSE(benchmark.update(130999));
  EXPECT_TRUE(benchmark.update(131000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::Complete);
  EXPECT_GE(benchmark.fullStats().elapsedMs, 60000u);
  EXPECT_GE(benchmark.windowStats().elapsedMs, 60000u);
}

TEST(RsvpWindowBenchmark, CountsActualKindsFallbacksAndBoundedDistributions) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  ASSERT_TRUE(benchmark.update(rsvp::RsvpWindowBenchmark::WARMUP_MS));
  benchmark.record(6000, 450, 400, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true);
  benchmark.record(7000, 550, 500, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::InvalidBaseline, true);
  benchmark.record(9000, 650, 600, rsvp::WindowActualRefresh::None, rsvp::WindowFallback::BusyTimeout, false);

  const auto& stats = benchmark.fullStats();
  EXPECT_EQ(stats.frame.count, 2u);
  EXPECT_EQ(stats.frame.minimumMs, 450u);
  EXPECT_EQ(stats.frame.meanMs(), 500u);
  EXPECT_EQ(stats.frame.maximumMs, 550u);
  EXPECT_EQ(stats.interval.count, 1u);
  EXPECT_EQ(stats.interval.minimumMs, 1000u);
  EXPECT_EQ(stats.interval.maximumMs, 1000u);
  EXPECT_EQ(stats.refresh.meanMs(), 450u);
  EXPECT_EQ(stats.fullCount, 2u);
  EXPECT_EQ(stats.fallbackCount, 2u);
  EXPECT_EQ(stats.failureCount, 1u);
  EXPECT_EQ(stats.lastFallback, rsvp::WindowFallback::BusyTimeout);
}

TEST(RsvpWindowBenchmark, DurationStatsFreezeCountAndTotalTogetherAtCapacity) {
  rsvp::WindowDurationStats stats;
  stats.count = UINT32_MAX;
  stats.minimumMs = 10;
  stats.maximumMs = 20;
  stats.totalMs = 1234;

  stats.record(UINT32_MAX);

  EXPECT_EQ(stats.count, UINT32_MAX);
  EXPECT_EQ(stats.minimumMs, 10u);
  EXPECT_EQ(stats.maximumMs, 20u);
  EXPECT_EQ(stats.totalMs, 1234u);
}

TEST(RsvpWindowBenchmark, CapturesCpuRangeOnlyFromSuccessfulMeasuredPresentations) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  ASSERT_TRUE(benchmark.update(5000));
  benchmark.record(6000, 100, 80, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true, 160);
  benchmark.record(7000, 100, 80, rsvp::WindowActualRefresh::None, rsvp::WindowFallback::BusyTimeout, false, 10);
  benchmark.record(8000, 100, 80, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true, 80);

  EXPECT_EQ(benchmark.fullStats().cpuSampleCount, 2u);
  EXPECT_EQ(benchmark.fullStats().minimumCpuMhz, 80u);
  EXPECT_EQ(benchmark.fullStats().maximumCpuMhz, 160u);
}

TEST(RsvpWindowBenchmark, ExcludesRepeatedSuccessfulAcknowledgementOfSameFrame) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  ASSERT_TRUE(benchmark.update(5000));
  benchmark.record(6000, 100, 80, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true, 160, 7);
  benchmark.record(7000, 120, 90, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true, 160, 7);

  EXPECT_EQ(benchmark.fullStats().frame.count, 1u);
  EXPECT_EQ(benchmark.fullStats().interval.count, 0u);
  EXPECT_EQ(benchmark.fullStats().fullCount, 1u);
}

TEST(RsvpWindowBenchmark, IgnoresWarmupSamplesAndResetsIntervalAtBranchBoundaries) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  benchmark.record(1000, 20, 10, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true);
  EXPECT_EQ(benchmark.fullStats().frame.count, 0u);
  ASSERT_TRUE(benchmark.update(5000));
  benchmark.record(5100, 20, 10, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true);
  EXPECT_EQ(benchmark.fullStats().interval.count, 0u);
  ASSERT_TRUE(benchmark.update(65000));
  ASSERT_TRUE(benchmark.update(70000));
  benchmark.record(70100, 20, 10, rsvp::WindowActualRefresh::Window, rsvp::WindowFallback::None, true);
  EXPECT_EQ(benchmark.windowStats().interval.count, 0u);
  EXPECT_EQ(benchmark.windowStats().windowCount, 1u);
}

TEST(RsvpWindowBenchmark, AbortAndFailureKeepPartialElapsedResults) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(10);
  ASSERT_TRUE(benchmark.update(5010));
  benchmark.abort(20010);
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::Aborted);
  EXPECT_EQ(benchmark.fullStats().elapsedMs, 15000u);

  benchmark.start(100);
  ASSERT_TRUE(benchmark.update(5100));
  benchmark.fail(10100);
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::Error);
  EXPECT_EQ(benchmark.fullStats().elapsedMs, 5000u);
}

TEST(RsvpWindowFixtureSource, ReopenRepeatsExactTextAndAnchors) {
  rsvp::RsvpWindowFixtureSource source;
  rsvp::DocumentEvent firstPass[6];
  ASSERT_TRUE(source.open());
  for (auto& event : firstPass) ASSERT_TRUE(source.next(event));

  ASSERT_TRUE(source.open());
  for (const auto& expected : firstPass) {
    rsvp::DocumentEvent actual;
    ASSERT_TRUE(source.next(actual));
    EXPECT_STREQ(actual.text, expected.text);
    EXPECT_EQ(actual.anchor.visibleTextOffset, expected.anchor.visibleTextOffset);
  }
}

TEST(RsvpWindowReport, WritesStableRowsOnlyWhenExplicitlyRequested) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  ASSERT_TRUE(benchmark.update(5000));
  benchmark.record(6000, 500, 450, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true, 160);
  benchmark.record(7000, 600, 550, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::InvalidBaseline, true, 80);
  StringSink sink;
  EXPECT_TRUE(rsvp::writeRsvpWindowReport(sink, benchmark,
                                          {.controller = "uc8253",
                                           .confidence = "assumed",
                                           .orientation = "portrait",
                                           .power = "not_sampled",
                                           .targetWpm = 300}));
  EXPECT_NE(sink.output.find("variant,controller,confidence"), std::string::npos);
  EXPECT_NE(sink.output.find("full,uc8253,assumed,portrait,not_sampled,300"), std::string::npos);
  EXPECT_NE(sink.output.find("window,uc8253,assumed,portrait,not_sampled,300"), std::string::npos);
  EXPECT_NE(sink.output.find("invalid_baseline"), std::string::npos);

  size_t lineStart = 0;
  uint8_t lineCount = 0;
  while (lineStart < sink.output.size()) {
    const size_t lineEnd = sink.output.find('\n', lineStart);
    ASSERT_NE(lineEnd, std::string::npos);
    EXPECT_EQ(std::count(sink.output.begin() + static_cast<std::string::difference_type>(lineStart),
                         sink.output.begin() + static_cast<std::string::difference_type>(lineEnd), ','),
              28);
    lineStart = lineEnd + 1;
    ++lineCount;
  }
  EXPECT_EQ(lineCount, 3u);
  EXPECT_NE(sink.output.find(",2,80,160,2,500,550,600,"), std::string::npos);
}

TEST(RsvpWindowReport, PropagatesStorageFailureWithoutChangingBenchmarkState) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(100);
  ASSERT_TRUE(benchmark.update(5100));
  benchmark.record(6000, 500, 450, rsvp::WindowActualRefresh::Full, rsvp::WindowFallback::None, true);
  const auto before = benchmark.fullStats();
  StringSink sink;
  sink.failWrites = true;

  EXPECT_FALSE(rsvp::writeRsvpWindowReport(sink, benchmark, {}));
  EXPECT_EQ(benchmark.fullStats().frame.count, before.frame.count);
  EXPECT_EQ(benchmark.fullStats().refresh.totalMs, before.refresh.totalMs);
}

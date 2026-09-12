#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "RsvpSession.h"
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
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::TightProbe);
  EXPECT_GE(benchmark.fullStats().elapsedMs, 60000u);
  EXPECT_GE(benchmark.windowStats().elapsedMs, 60000u);
  benchmark.startTight(131000);
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::TightWarmup);
  EXPECT_TRUE(benchmark.update(136000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::TightMeasurement);
  EXPECT_TRUE(benchmark.update(196000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::Complete);
  EXPECT_GE(benchmark.tightStats().elapsedMs, 60000u);
}

TEST(RsvpWindowBenchmark, KeepsWindowGeometrySamplesSeparateForEachMeasuredBranch) {
  rsvp::RsvpWindowBenchmark benchmark;
  benchmark.start(0);
  ASSERT_TRUE(benchmark.update(5000));
  ASSERT_TRUE(benchmark.update(65000));
  ASSERT_TRUE(benchmark.update(70000));
  benchmark.record(7010, 100, 80, rsvp::WindowActualRefresh::Window, rsvp::WindowFallback::None, true, 160, 1);
  benchmark.recordRoi(80, 12);
  benchmark.record(7020, 100, 80, rsvp::WindowActualRefresh::Window, rsvp::WindowFallback::None, true, 160, 2);
  benchmark.recordRoi(40, 8);
  ASSERT_TRUE(benchmark.update(130000));
  EXPECT_EQ(benchmark.currentPhase(), rsvp::WindowBenchmarkPhase::TightProbe);

  benchmark.startTight(130000);
  ASSERT_TRUE(benchmark.update(135000));
  benchmark.record(135010, 90, 70, rsvp::WindowActualRefresh::Window, rsvp::WindowFallback::None, true, 160, 3);
  benchmark.recordRoi(24, 6);
  benchmark.recordRoi(0, 0);

  EXPECT_EQ(benchmark.fullStats().roi.sampleCount, 0u);
  EXPECT_EQ(benchmark.windowStats().roi.sampleCount, 2u);
  EXPECT_EQ(benchmark.windowStats().roi.minimumWidth, 40u);
  EXPECT_EQ(benchmark.windowStats().roi.maximumWidth, 80u);
  EXPECT_EQ(benchmark.windowStats().roi.minimumHeight, 8u);
  EXPECT_EQ(benchmark.windowStats().roi.maximumHeight, 12u);
  EXPECT_EQ(benchmark.tightRoiStats().sampleCount, 1u);
  EXPECT_EQ(benchmark.tightRoiStats().minimumWidth, 24u);
  EXPECT_EQ(benchmark.tightRoiStats().maximumWidth, 24u);
  EXPECT_EQ(benchmark.tightRoiStats().minimumHeight, 6u);
  EXPECT_EQ(benchmark.tightRoiStats().maximumHeight, 6u);
}

TEST(RsvpWindowBenchmark, UnlimitedDiagnosticPacingLeavesNoSessionDelay) {
  rsvp::RsvpWindowFixtureSource source;
  rsvp::RsvpPacingConfig pacing;
  pacing.paceWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  pacing.minimumWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  pacing.maximumWpm = pacing.safeMaximumWpm = rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM;
  rsvp::RsvpSession session(source, {}, pacing, false);

  const auto first = session.step({.nowMs = 100});
  ASSERT_TRUE(first.render);
  EXPECT_EQ(first.paceWpm, rsvp::RsvpWindowBenchmark::UNLIMITED_PACE_WPM);
  const auto acknowledged = session.step({.nowMs = 100,
                                          .action = rsvp::Action::FramePresented,
                                          .presentedFrameId = first.frame.id,
                                          .refreshDurationMs = 0});
  ASSERT_TRUE(acknowledged.presentationAccepted);
  EXPECT_EQ(acknowledged.nextDeadlineMs, 100u);
  ASSERT_EQ(session.step({.nowMs = 100, .action = rsvp::Action::TogglePlayback}).state, rsvp::State::Playing);
  EXPECT_TRUE(session.step({.nowMs = 101}).render);
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

TEST(RsvpWindowBenchmark, KeepsExactFailureSnapshotAfterRecovery) {
  rsvp::WindowDiagnosticState diagnostic;
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::FullWarmup, rsvp::WindowDiagnosticOperation::FullSizePtl,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::None, 0, 12, true);
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::FullWarmup, rsvp::WindowDiagnosticOperation::FullSizePtl,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::None, 0, 14, true);
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::FullWarmup, rsvp::WindowDiagnosticOperation::FullSizePtl,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::None,
                           rsvp::WindowDiagnosticError::BusyNotReady, 7, 23, false);

  ASSERT_TRUE(diagnostic.lastFailure.valid);
  EXPECT_EQ(diagnostic.lastOperation, rsvp::WindowDiagnosticOperation::FullSizePtl);
  EXPECT_EQ(diagnostic.lastSuccessfulOperation, rsvp::WindowDiagnosticOperation::FullSizePtl);
  EXPECT_EQ(diagnostic.fullWarmupSuccesses, 2u);
  EXPECT_EQ(diagnostic.lastFailure.phase, rsvp::WindowDiagnosticPhase::FullWarmup);
  EXPECT_EQ(diagnostic.lastFailure.operation, rsvp::WindowDiagnosticOperation::FullSizePtl);
  EXPECT_EQ(diagnostic.lastFailure.requested, rsvp::WindowActualRefresh::Full);
  EXPECT_EQ(diagnostic.lastFailure.actual, rsvp::WindowActualRefresh::None);
  EXPECT_EQ(diagnostic.lastFailure.error, rsvp::WindowDiagnosticError::BusyNotReady);
  EXPECT_EQ(diagnostic.lastFailure.rawError, 7u);
  EXPECT_EQ(diagnostic.lastFailure.durationMs, 23u);
  EXPECT_EQ(diagnostic.lastFailure.warmupSuccesses, 2u);
  EXPECT_EQ(diagnostic.lastFailure.lastSuccessfulOperation, rsvp::WindowDiagnosticOperation::FullSizePtl);

  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::Error, rsvp::WindowDiagnosticOperation::FullRecovery,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::BusyTimeout, 8, 32, false);

  EXPECT_EQ(diagnostic.lastOperation, rsvp::WindowDiagnosticOperation::FullRecovery);
  EXPECT_EQ(diagnostic.lastSuccessfulOperation, rsvp::WindowDiagnosticOperation::FullSizePtl);
  ASSERT_TRUE(diagnostic.lastFailure.valid);
  EXPECT_EQ(diagnostic.lastFailure.error, rsvp::WindowDiagnosticError::BusyNotReady);
  EXPECT_EQ(diagnostic.lastFailure.rawError, 7u);
  EXPECT_EQ(diagnostic.lastFailure.durationMs, 23u);

  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::Error, rsvp::WindowDiagnosticOperation::FullRecovery,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::None, 0, 31, true);

  EXPECT_EQ(diagnostic.lastOperation, rsvp::WindowDiagnosticOperation::FullRecovery);
  EXPECT_EQ(diagnostic.lastSuccessfulOperation, rsvp::WindowDiagnosticOperation::FullRecovery);
  EXPECT_EQ(diagnostic.warmupSuccessCount(), 2u);
  ASSERT_TRUE(diagnostic.lastFailure.valid);
  EXPECT_EQ(diagnostic.lastFailure.error, rsvp::WindowDiagnosticError::BusyNotReady);
  EXPECT_EQ(diagnostic.lastFailure.durationMs, 23u);
  EXPECT_EQ(diagnostic.lastFailure.lastSuccessfulOperation, rsvp::WindowDiagnosticOperation::FullSizePtl);
}

TEST(RsvpWindowBenchmark, CapturesTightWarmupContextInFailureSnapshot) {
  rsvp::WindowDiagnosticState diagnostic;
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::TightWarmup, rsvp::WindowDiagnosticOperation::TightWindow,
                           rsvp::WindowActualRefresh::Window, rsvp::WindowActualRefresh::Window,
                           rsvp::WindowDiagnosticError::None, 0, 18, true);
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::TightWarmup, rsvp::WindowDiagnosticOperation::TightWindow,
                           rsvp::WindowActualRefresh::Window, rsvp::WindowActualRefresh::None,
                           rsvp::WindowDiagnosticError::BusyTimeout, 11, 29, false);

  EXPECT_EQ(diagnostic.tightWarmupSuccesses, 1u);
  EXPECT_EQ(diagnostic.warmupSuccessCount(), 1u);
  ASSERT_TRUE(diagnostic.lastFailure.valid);
  EXPECT_EQ(diagnostic.lastFailure.phase, rsvp::WindowDiagnosticPhase::TightWarmup);
  EXPECT_EQ(diagnostic.lastFailure.warmupSuccesses, 1u);
}

TEST(RsvpWindowFixtureSource, ReopenRepeatsExactTextAndAnchors) {
  rsvp::RsvpWindowFixtureSource source;
  rsvp::DocumentEvent firstPass[6];
  ASSERT_TRUE(source.open());
  for (auto& event : firstPass) ASSERT_TRUE(source.next(event));
  EXPECT_STREQ(firstPass[1].text, "a");
  EXPECT_STREQ(firstPass[3].text, "to");

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
  rsvp::WindowDiagnosticState diagnostic;
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::FullWarmup, rsvp::WindowDiagnosticOperation::FullSizePtl,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::None, 0, 10, true);
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::FullWarmup, rsvp::WindowDiagnosticOperation::FullSizePtl,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::None,
                           rsvp::WindowDiagnosticError::BusyNotReady, 9, 27, false);
  diagnostic.recordAttempt(rsvp::WindowDiagnosticPhase::Error, rsvp::WindowDiagnosticOperation::FullRecovery,
                           rsvp::WindowActualRefresh::Full, rsvp::WindowActualRefresh::Full,
                           rsvp::WindowDiagnosticError::None, 0, 30, true);
  diagnostic.lastFailure.trace = {.valid = true,
                                  .phase = 2,
                                  .stage = 3,
                                  .error = 2,
                                  .wait = 1,
                                  .busyBefore = true,
                                  .busyAfter = true,
                                  .baselineBefore = 2,
                                  .baselineAfter = 0,
                                  .x = 8,
                                  .y = 10,
                                  .width = 16,
                                  .height = 4,
                                  .payloadBytes = 1234,
                                  .refreshTriggered = false};
  StringSink sink;
  EXPECT_TRUE(rsvp::writeRsvpWindowReport(sink, benchmark,
                                          {.controller = "uc8253",
                                           .confidence = "assumed",
                                           .orientation = "portrait",
                                           .power = "not_sampled",
                                           .targetWpm = 300,
                                           .firmwareVersion = "0.9.1-windowtest.1",
                                           .diagnosticStage = "error",
                                           .diagnosticStatus = "awaiting_recovery",
                                           .fullProbeConfirmed = true,
                                           .lineProbeConfirmed = false,
                                           .tightProbeConfirmed = true},
                                          diagnostic));
  EXPECT_NE(sink.output.find("variant,controller,confidence"), std::string::npos);
  EXPECT_NE(sink.output.find("full,uc8253,assumed,portrait,not_sampled,300"), std::string::npos);
  EXPECT_NE(sink.output.find("window,uc8253,assumed,portrait,not_sampled,300"), std::string::npos);
  EXPECT_NE(sink.output.find("tight_window,uc8253,assumed,portrait,not_sampled,300"), std::string::npos);
  EXPECT_NE(sink.output.find(
                "tight_probe_confirmed,roi_sample_count,roi_min_width,roi_max_width,roi_min_height,roi_max_height"),
            std::string::npos);
  EXPECT_NE(sink.output.find("invalid_baseline"), std::string::npos);
  EXPECT_NE(sink.output.find("last_operation,last_successful_operation,full_warmup_successes,window_warmup_successes"),
            std::string::npos);
  EXPECT_NE(sink.output.find("busy_not_ready"), std::string::npos);
  EXPECT_NE(sink.output.find(",1,0,full_warmup,full_size_ptl"), std::string::npos);
  EXPECT_NE(sink.output.find("0.9.1-windowtest.1,error,awaiting_recovery,1,0,1,2,3,2,1,1,1,2,0,8,10,16,4,1234,0"),
            std::string::npos);

  size_t lineStart = 0;
  uint8_t lineCount = 0;
  while (lineStart < sink.output.size()) {
    const size_t lineEnd = sink.output.find('\n', lineStart);
    ASSERT_NE(lineEnd, std::string::npos);
    EXPECT_EQ(std::count(sink.output.begin() + static_cast<std::string::difference_type>(lineStart),
                         sink.output.begin() + static_cast<std::string::difference_type>(lineEnd), ','),
              67);
    lineStart = lineEnd + 1;
    ++lineCount;
  }
  EXPECT_EQ(lineCount, 4u);
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

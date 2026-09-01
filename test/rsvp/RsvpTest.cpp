#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "EpubContentProvider.h"
#include "EpubVisibleTextSource.h"
#include "RsvpRefreshStats.h"
#include "RsvpSession.h"
#include "VisibleContentStream.h"

namespace {

class CollectingSink final : public rsvp::EventSink {
 public:
  bool onEvent(const rsvp::DocumentEvent& event) override {
    events.push_back(event);
    return true;
  }

  std::vector<rsvp::DocumentEvent> events;
};

class VectorSource final : public rsvp::RsvpSource {
 public:
  explicit VectorSource(std::vector<rsvp::DocumentEvent> events) : events(std::move(events)) {}

  bool open(const rsvp::ResumeAnchor*) override {
    index = 0;
    return openSucceeds;
  }

  bool next(rsvp::DocumentEvent& out) override {
    if (index >= events.size()) return false;
    out = events[index++];
    return true;
  }

  bool openSucceeds = true;

 private:
  std::vector<rsvp::DocumentEvent> events;
  size_t index = 0;
};

class StringEpubProvider final : public rsvp::EpubContentProvider {
 public:
  explicit StringEpubProvider(std::vector<std::string> spines) : spines(std::move(spines)) {}

  int spineCount() const override { return static_cast<int>(spines.size()); }

  bool streamSpine(const uint16_t spineIndex, rsvp::ContentByteSink& sink, const size_t chunkSize) override {
    if (spineIndex >= spines.size()) return false;
    const auto& content = spines[spineIndex];
    for (size_t offset = 0; offset < content.size();) {
      const size_t length = std::min({chunkSize, size_t{5}, content.size() - offset});
      const size_t written = sink.write(reinterpret_cast<const uint8_t*>(content.data() + offset), length);
      if (written != length) return true;
      offset += length;
    }
    return true;
  }

 private:
  std::vector<std::string> spines;
};

class RecordingDisplay final {
 public:
  void present(const rsvp::Decision& decision) {
    ASSERT_TRUE(decision.render);
    frames.emplace_back(decision.frame.text, decision.frame.textLength);
    frameIds.push_back(decision.frame.id);
    requestedAt.push_back(decision.frame.requestedAtMs);
  }

  std::vector<std::string> frames;
  std::vector<uint32_t> frameIds;
  std::vector<uint32_t> requestedAt;
};

rsvp::DocumentEvent word(const char* text, const uint32_t offset = 0) {
  rsvp::DocumentEvent event;
  event.kind = rsvp::EventKind::Word;
  event.anchor.visibleTextOffset = offset;
  event.anchor.valid = true;
  event.textLength = static_cast<uint16_t>(strlen(text));
  memcpy(event.text, text, event.textLength + 1);
  return event;
}

std::vector<rsvp::DocumentEvent> parse(const std::string& xhtml, const size_t chunkSize) {
  CollectingSink sink;
  rsvp::VisibleContentStream stream(3, sink);
  for (size_t pos = 0; pos < xhtml.size();) {
    const size_t length = std::min(chunkSize, xhtml.size() - pos);
    EXPECT_EQ(stream.write(reinterpret_cast<const uint8_t*>(xhtml.data() + pos), length), length);
    pos += length;
  }
  EXPECT_TRUE(stream.finish());
  return sink.events;
}

uint16_t little16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1] << 8);
}

uint32_t little32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::string readStoredZipEntry(const char* archivePath, const std::string& wantedName) {
  std::ifstream input(archivePath, std::ios::binary);
  if (!input) return {};
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  size_t offset = 0;
  while (offset + 30 <= bytes.size() && little32(bytes.data() + offset) == 0x04034b50) {
    const uint16_t method = little16(bytes.data() + offset + 8);
    const uint32_t compressedSize = little32(bytes.data() + offset + 18);
    const uint16_t nameLength = little16(bytes.data() + offset + 26);
    const uint16_t extraLength = little16(bytes.data() + offset + 28);
    const size_t dataOffset = offset + 30 + nameLength + extraLength;
    if (dataOffset + compressedSize > bytes.size()) return {};
    const std::string name(reinterpret_cast<const char*>(bytes.data() + offset + 30), nameLength);
    if (name == wantedName && method == 0) {
      return {reinterpret_cast<const char*>(bytes.data() + dataOffset), compressedSize};
    }
    offset = dataOffset + compressedSize;
  }
  return {};
}

class StoredFixtureEpubProvider final : public rsvp::EpubContentProvider {
 public:
  explicit StoredFixtureEpubProvider(const char* archivePath) : archivePath(archivePath) {}

  int spineCount() const override { return 1; }

  bool streamSpine(const uint16_t spineIndex, rsvp::ContentByteSink& sink, const size_t chunkSize) override {
    if (spineIndex != 0) return false;
    const std::string content = readStoredZipEntry(archivePath, "OEBPS/chapter.xhtml");
    if (content.empty()) return false;
    for (size_t offset = 0; offset < content.size();) {
      const size_t length = std::min(chunkSize, content.size() - offset);
      const size_t written = sink.write(reinterpret_cast<const uint8_t*>(content.data() + offset), length);
      if (written != length) return true;
      offset += length;
    }
    return true;
  }

 private:
  const char* archivePath;
};

TEST(RsvpSession, FirstStepPresentsFirstLexicalWordPaused) {
  VectorSource source({word("Привет", 4), word("мир", 11)});
  rsvp::RsvpSession session(source, {.spineIndex = 2, .visibleTextOffset = 4, .valid = true});

  const auto decision = session.step({.nowMs = 100});

  EXPECT_EQ(decision.state, rsvp::State::Paused);
  EXPECT_TRUE(decision.render);
  ASSERT_NE(decision.frame.text, nullptr);
  EXPECT_EQ(std::string(decision.frame.text, decision.frame.textLength), "Привет");
  EXPECT_EQ(decision.frame.anchor.spineIndex, 0);
  EXPECT_EQ(decision.frame.anchor.visibleTextOffset, 4u);
}

TEST(RsvpSession, FakeTimeAndRecordedPresentationAreAcknowledged) {
  VectorSource source({word("Первое")});
  rsvp::RsvpSession session(source);
  RecordingDisplay display;

  const auto frame = session.step({.nowMs = 120});
  display.present(frame);
  const auto acknowledgement = session.step({.nowMs = 455,
                                             .action = rsvp::Action::FramePresented,
                                             .presentedFrameId = display.frameIds[0],
                                             .refreshDurationMs = 335});

  ASSERT_EQ(display.frames.size(), 1u);
  EXPECT_EQ(display.frames[0], "Первое");
  EXPECT_EQ(display.requestedAt[0], 120u);
  EXPECT_TRUE(acknowledgement.presentationAccepted);
  EXPECT_EQ(acknowledgement.presentedAtMs, 455u);
  EXPECT_EQ(acknowledgement.refreshDurationMs, 335u);
}

TEST(RsvpSession, ModeSwitchRequestsPagedMode) {
  VectorSource source({word("Первое")});
  rsvp::RsvpSession session(source);
  ASSERT_TRUE(session.step({}).render);

  const auto decision = session.step({.nowMs = 10, .action = rsvp::Action::ModeSwitch});

  EXPECT_EQ(decision.state, rsvp::State::Exited);
  EXPECT_TRUE(decision.switchToPaged);
}

TEST(RsvpSession, UnsupportedFirstElementRequestsPagedFallback) {
  rsvp::DocumentEvent image;
  image.kind = rsvp::EventKind::NonText;
  image.nonText = rsvp::NonTextKind::Image;
  VectorSource source({image});
  rsvp::RsvpSession session(source);

  const auto decision = session.step({});

  EXPECT_EQ(decision.state, rsvp::State::Boundary);
  EXPECT_TRUE(decision.pagedModeAvailable);
  EXPECT_FALSE(decision.switchToPaged);
  EXPECT_FALSE(decision.render);
}

TEST(RsvpSession, SourceOpenFailureIsExplicit) {
  VectorSource source({});
  source.openSucceeds = false;
  rsvp::RsvpSession session(source);

  const auto decision = session.step({});

  EXPECT_EQ(decision.state, rsvp::State::Error);
  EXPECT_EQ(decision.error, rsvp::Error::SourceOpen);
  EXPECT_FALSE(decision.render);
}

TEST(VisibleContentStream, PreservesRussianWordOrderAndUnicodeOffsetsAcrossChunks) {
  const auto events = parse("<html><body><p>" + std::string("Привет мир") + "</p></body></html>", 3);

  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].kind, rsvp::EventKind::Word);
  EXPECT_STREQ(events[0].text, "Привет");
  EXPECT_EQ(events[0].anchor.spineIndex, 3);
  EXPECT_EQ(events[0].anchor.visibleTextOffset, 0u);
  EXPECT_EQ(events[1].kind, rsvp::EventKind::Word);
  EXPECT_STREQ(events[1].text, "мир");
  EXPECT_EQ(events[1].anchor.visibleTextOffset, 7u);
  EXPECT_EQ(events[2].kind, rsvp::EventKind::ParagraphBoundary);
  EXPECT_EQ(events[2].anchor.visibleTextOffset, 10u);
}

TEST(VisibleContentStream, NormalizesCrLfAndSplitsResolvedUnicodeWhitespace) {
  const auto events = parse("<body>один\r\nдва&nbsp;три</body>", 2);

  ASSERT_EQ(events.size(), 4u);
  EXPECT_STREQ(events[0].text, "один");
  EXPECT_EQ(events[0].anchor.visibleTextOffset, 0u);
  EXPECT_STREQ(events[1].text, "два");
  EXPECT_EQ(events[1].anchor.visibleTextOffset, 5u);
  EXPECT_STREQ(events[2].text, "три");
  EXPECT_EQ(events[2].anchor.visibleTextOffset, 9u);
  EXPECT_EQ(events[3].kind, rsvp::EventKind::ParagraphBoundary);
  EXPECT_EQ(events[3].anchor.visibleTextOffset, 12u);
}

TEST(VisibleContentStream, MarksPunctuationOnlyRunsAsNonLexical) {
  const auto events = parse("<body>— &hellip; Первое</body>", 3);

  ASSERT_EQ(events.size(), 4u);
  EXPECT_EQ(events[0].kind, rsvp::EventKind::NonLexicalText);
  EXPECT_EQ(events[1].kind, rsvp::EventKind::NonLexicalText);
  EXPECT_EQ(events[2].kind, rsvp::EventKind::Word);
  EXPECT_STREQ(events[2].text, "Первое");
}

TEST(VisibleContentStream, SplitsEmDashFromThePreviousWordAndKeepsItVisible) {
  const auto events = parse("<body>слово—следующее</body>", 3);

  ASSERT_EQ(events.size(), 3u);
  EXPECT_STREQ(events[0].text, "слово");
  EXPECT_STREQ(events[1].text, "—следующее");
  EXPECT_EQ(events[1].anchor.visibleTextOffset, 5u);
}

TEST(VisibleContentStream, RejoinsDiscretionaryAndLayoutOnlyHyphenation) {
  const auto events = parse("<body>пере&shy; нос обыч-\r\nный</body>", 3);

  ASSERT_EQ(events.size(), 3u);
  EXPECT_STREQ(events[0].text, "перенос");
  EXPECT_STREQ(events[1].text, "обычный");
}

TEST(VisibleContentStream, IgnoresNonVisibleTextAndReportsDocumentElements) {
  const auto events = parse(
      "<html><head><title>hidden</title></head><body><script>hidden()</script>alpha<img src='x'/>beta<hr/></body>"
      "</html>",
      7);

  ASSERT_EQ(events.size(), 5u);
  EXPECT_STREQ(events[0].text, "alpha");
  EXPECT_EQ(events[1].kind, rsvp::EventKind::NonText);
  EXPECT_EQ(events[1].nonText, rsvp::NonTextKind::Image);
  EXPECT_STREQ(events[2].text, "beta");
  EXPECT_EQ(events[3].kind, rsvp::EventKind::NonText);
  EXPECT_EQ(events[3].nonText, rsvp::NonTextKind::HorizontalRule);
  EXPECT_EQ(events[4].kind, rsvp::EventKind::ParagraphBoundary);
}

TEST(VisibleContentStream, ReportsUnsupportedRichDocumentElements) {
  const auto events = parse("<body>alpha<svg><text>hidden</text></svg></body>", 4);

  ASSERT_GE(events.size(), 2u);
  EXPECT_STREQ(events[0].text, "alpha");
  EXPECT_EQ(events[1].kind, rsvp::EventKind::NonText);
  EXPECT_EQ(events[1].nonText, rsvp::NonTextKind::Other);
}

TEST(VisibleContentStream, ReportsOversizedWordsWithoutGrowingTheBuffer) {
  const std::string oversized(rsvp::MAX_TOKEN_BYTES + 20, 'a');
  const auto events = parse("<body>" + oversized + " ok</body>", 11);

  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].kind, rsvp::EventKind::OversizedWord);
  EXPECT_EQ(events[0].anchor.visibleTextOffset, 0u);
  EXPECT_EQ(events[1].kind, rsvp::EventKind::Word);
  EXPECT_STREQ(events[1].text, "ok");
}

TEST(RsvpRefreshStats, KeepsSeparateBoundedDistributions) {
  rsvp::RsvpRefreshStats stats;
  stats.record(rsvp::RefreshKind::Fast, 210);
  stats.record(rsvp::RefreshKind::Fast, 540);
  stats.record(rsvp::RefreshKind::Cleanup, 1720);

  const auto& fast = stats.distribution(rsvp::RefreshKind::Fast);
  EXPECT_EQ(fast.count, 2u);
  EXPECT_EQ(fast.minimumMs, 210u);
  EXPECT_EQ(fast.maximumMs, 540u);
  EXPECT_EQ(fast.averageMs(), 375u);
  EXPECT_EQ(fast.buckets[0], 1u);
  EXPECT_EQ(fast.buckets[2], 1u);

  const auto& cleanup = stats.distribution(rsvp::RefreshKind::Cleanup);
  EXPECT_EQ(cleanup.count, 1u);
  EXPECT_EQ(cleanup.buckets[4], 1u);
}

TEST(RsvpFixture, RealEpubChapterFlowsFromSourceToPausedFirstToken) {
  StoredFixtureEpubProvider provider(RSVP_FIXTURE_EPUB);
  rsvp::EpubVisibleTextSource source(provider);
  rsvp::RsvpSession session(source);

  const auto decision = session.step({});

  ASSERT_TRUE(decision.render);
  EXPECT_EQ(decision.state, rsvp::State::Paused);
  EXPECT_EQ(std::string(decision.frame.text, decision.frame.textLength), "Первое");
  EXPECT_EQ(decision.frame.anchor.visibleTextOffset, 0u);
}

TEST(EpubVisibleTextSource, EmitsChapterBoundaryWithoutCollectingSpines) {
  StringEpubProvider provider({"<body>один</body>", "<body>два</body>"});
  rsvp::EpubVisibleTextSource source(provider);
  ASSERT_TRUE(source.open());

  rsvp::DocumentEvent event;
  ASSERT_TRUE(source.next(event));
  EXPECT_EQ(event.kind, rsvp::EventKind::Word);
  EXPECT_STREQ(event.text, "один");
  ASSERT_TRUE(source.next(event));
  EXPECT_EQ(event.kind, rsvp::EventKind::ParagraphBoundary);
  ASSERT_TRUE(source.next(event));
  EXPECT_EQ(event.kind, rsvp::EventKind::ChapterBoundary);
  EXPECT_EQ(event.anchor.spineIndex, 1);
  ASSERT_TRUE(source.next(event));
  EXPECT_EQ(event.kind, rsvp::EventKind::Word);
  EXPECT_STREQ(event.text, "два");
}

TEST(EpubVisibleTextSource, ResumesAtTheExactSameOffsetOrdinal) {
  StringEpubProvider provider({"<body><img src='cover'/>слово</body>"});
  rsvp::EpubVisibleTextSource source(provider);
  const rsvp::ResumeAnchor anchor{.spineIndex = 0, .visibleTextOffset = 0, .sameOffsetOrdinal = 1, .valid = true};
  ASSERT_TRUE(source.open(&anchor));

  rsvp::DocumentEvent event;
  ASSERT_TRUE(source.next(event));
  EXPECT_EQ(event.kind, rsvp::EventKind::Word);
  EXPECT_STREQ(event.text, "слово");
  EXPECT_EQ(event.anchor.sameOffsetOrdinal, 1u);
}

TEST(EpubVisibleTextSource, RejectsResumeAnchorOutsideTheCurrentBookRevision) {
  StringEpubProvider provider({"<html><body><p>first</p></body></html>"});
  rsvp::EpubVisibleTextSource source(provider);
  const rsvp::ResumeAnchor invalid{.spineIndex = 9, .visibleTextOffset = 0, .valid = true};

  EXPECT_FALSE(source.open(&invalid));
}

}  // namespace

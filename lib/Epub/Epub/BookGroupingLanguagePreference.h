#pragma once

#include <cstdint>
#include <string>
#include <utility>

// Small per-book override stored beside the derived EPUB metadata. The value
// is deliberately independent of book.bin so rebuilding derived caches cannot
// discard a user's choice or durable reading position.
class BookGroupingLanguagePreference final {
 public:
  // Values match rsvp::GroupingLanguageChoice and are kept as a byte on disk.
  enum class Choice : uint8_t { Auto = 0, Russian = 1, English = 2 };

  explicit BookGroupingLanguagePreference(std::string cachePath) : cachePath_(std::move(cachePath)) {}

  Choice load() const;
  bool save(Choice choice);

 private:
  std::string cachePath_;
};

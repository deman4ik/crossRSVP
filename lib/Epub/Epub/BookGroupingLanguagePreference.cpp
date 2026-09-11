#include "BookGroupingLanguagePreference.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>

namespace {
constexpr std::array<uint8_t, 4> MAGIC = {'C', 'P', 'G', 'L'};
constexpr uint8_t VERSION = 1;
constexpr size_t FILE_SIZE = 6;
constexpr char FILE_NAME[] = "/grouping-language.bin";
constexpr char TEMP_NAME[] = "/grouping-language.bin.tmp";
constexpr char BACKUP_NAME[] = "/grouping-language.bin.bak";

bool validChoice(const uint8_t value) {
  return value <= static_cast<uint8_t>(BookGroupingLanguagePreference::Choice::English);
}
}  // namespace

BookGroupingLanguagePreference::Choice BookGroupingLanguagePreference::load() const {
  HalFile file;
  const auto path = cachePath_ + FILE_NAME;
  const auto backup = cachePath_ + BACKUP_NAME;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("BGL", path, file)) {
    // A reset between the backup and temporary-file promotions can leave only
    // the last known-good backup. Recover it lazily on the next open.
    if (Storage.exists(backup.c_str()) && Storage.openFileForRead("BGL", backup, file)) {
      file.close();
      if (!Storage.rename(backup.c_str(), path.c_str())) return Choice::Auto;
      if (!Storage.openFileForRead("BGL", path, file)) return Choice::Auto;
    } else {
      return Choice::Auto;
    }
  }
  if (file.fileSize() != FILE_SIZE) {
    return Choice::Auto;
  }

  uint8_t bytes[FILE_SIZE] = {};
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes) || !std::equal(MAGIC.begin(), MAGIC.end(), bytes) ||
      bytes[4] != VERSION || !validChoice(bytes[5])) {
    LOG_DBG("BGL", "Ignoring invalid grouping preference");
    return Choice::Auto;
  }
  return static_cast<Choice>(bytes[5]);
}

bool BookGroupingLanguagePreference::save(const Choice choice) {
  const auto value = static_cast<uint8_t>(choice);
  if (!validChoice(value)) return false;
  if (load() == choice) return true;

  Storage.mkdir(cachePath_.c_str());
  HalFile file;
  if (!Storage.openFileForWrite("BGL", cachePath_ + TEMP_NAME, file)) return false;
  const uint8_t bytes[FILE_SIZE] = {MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3], VERSION, value};
  if (file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
    file.close();
    Storage.remove((cachePath_ + TEMP_NAME).c_str());
    return false;
  }
  file.close();
  const auto path = cachePath_ + FILE_NAME;
  const auto temporary = cachePath_ + TEMP_NAME;
  const auto backup = cachePath_ + BACKUP_NAME;
  const bool hadPrevious = Storage.exists(path.c_str());
  if (Storage.exists(backup.c_str())) Storage.remove(backup.c_str());
  if (hadPrevious && !Storage.rename(path.c_str(), backup.c_str())) {
    Storage.remove(temporary.c_str());
    return false;
  }
  if (!Storage.rename(temporary.c_str(), path.c_str())) {
    if (hadPrevious) Storage.rename(backup.c_str(), path.c_str());
    Storage.remove(temporary.c_str());
    LOG_ERR("BGL", "Failed to promote grouping preference");
    return false;
  }
  if (hadPrevious) Storage.remove(backup.c_str());
  return true;
}

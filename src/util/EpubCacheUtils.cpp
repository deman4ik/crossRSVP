#include "EpubCacheUtils.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <string>

namespace {

bool isDurableReaderState(const char* name) {
  return std::strcmp(name, "progress.bin") == 0 || std::strcmp(name, "rsvp_checkpoint.bin") == 0 ||
         std::strcmp(name, "rsvp_checkpoint.bin.bak") == 0;
}

}  // namespace

bool clearEpubDerivedCache(const std::string& cachePath) {
  if (!Storage.exists(cachePath.c_str())) {
    return true;
  }

  auto root = Storage.open(cachePath.c_str());
  if (!root || !root.isDirectory()) {
    LOG_ERR("EpubCache", "Failed to open cache directory: %s", cachePath.c_str());
    return false;
  }

  bool success = true;
  char name[128];
  // Cache maintenance is cold-path work and cache paths are not bounded. One
  // reserved string avoids both a large stack buffer and per-entry growth.
  std::string entryPath;
  entryPath.reserve(cachePath.size() + sizeof(name) + 1);
  for (auto entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    const size_t nameLength = entry.getName(name, sizeof(name));
    const bool isDirectory = entry.isDirectory();
    entry.close();

    if (nameLength == 0) {
      success = false;
      continue;
    }
    if (isDurableReaderState(name)) {
      continue;
    }

    entryPath.assign(cachePath);
    entryPath.push_back('/');
    entryPath.append(name);
    const bool removed = isDirectory ? Storage.removeDir(entryPath.c_str()) : Storage.remove(entryPath.c_str());
    if (!removed) {
      LOG_ERR("EpubCache", "Failed to remove derived cache entry: %s", entryPath.c_str());
      success = false;
    }
  }
  root.close();
  return success;
}

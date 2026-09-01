#include "RsvpCheckpointFile.h"

#include <HalStorage.h>

#include <array>

namespace rsvp {
namespace {

constexpr const char* kCheckpointName = "/rsvp_checkpoint.bin";

std::string checkpointPath(const std::string& cachePath) { return cachePath + kCheckpointName; }

CheckpointStatus readCheckpoint(const std::string& path, const uint64_t expectedRevision,
                                RsvpCheckpoint& checkpoint) {
  if (!Storage.exists(path.c_str())) return CheckpointStatus::Missing;
  HalFile file;
  if (!Storage.openFileForRead("RSVP", path, file)) return CheckpointStatus::ReadError;

  std::array<uint8_t, RsvpCheckpointCodec::kEncodedSize + 1> bytes{};
  const int size = file.read(bytes.data(), bytes.size());
  if (size < 0) return CheckpointStatus::Corrupt;
  if (size == 0) return CheckpointStatus::Truncated;
  return RsvpCheckpointCodec::decode(bytes.data(), static_cast<size_t>(size), expectedRevision, checkpoint);
}

bool sameCheckpoint(const RsvpCheckpoint& left, const RsvpCheckpoint& right) {
  return left.bookRevision == right.bookRevision && left.anchor.spineIndex == right.anchor.spineIndex &&
         left.anchor.visibleTextOffset == right.anchor.visibleTextOffset &&
         left.anchor.sameOffsetOrdinal == right.anchor.sameOffsetOrdinal &&
         left.anchor.valid == right.anchor.valid && left.tokenHash32 == right.tokenHash32 &&
         left.tokenLength == right.tokenLength && left.activeRsvpTimeMs == right.activeRsvpTimeMs;
}

}  // namespace

bool RsvpCheckpointFile::computeBookRevision(const std::string& bookPath, uint64_t& revision) {
  HalFile file;
  if (!Storage.openFileForRead("RSVP", bookPath, file)) return false;

  // FNV-1a over the complete EPUB makes replacement at the same path stale,
  // including changes not represented by file size or timestamps.
  uint64_t hash = 14695981039346656037ULL;
  std::array<uint8_t, 1024> buffer{};
  while (true) {
    const int count = file.read(buffer.data(), buffer.size());
    if (count < 0) return false;
    if (count == 0) break;
    for (int index = 0; index < count; ++index) {
      hash ^= buffer[static_cast<size_t>(index)];
      hash *= 1099511628211ULL;
    }
  }
  revision = hash;
  return true;
}

CheckpointStatus RsvpCheckpointFile::load(const std::string& cachePath, const uint64_t expectedRevision,
                                          RsvpCheckpoint& checkpoint) {
  const std::string path = checkpointPath(cachePath);
  const std::string backup = path + ".bak";
  if (!Storage.exists(path.c_str()) && Storage.exists(backup.c_str())) {
    Storage.rename(backup.c_str(), path.c_str());
  }

  const auto status = readCheckpoint(path, expectedRevision, checkpoint);
  if (status == CheckpointStatus::Ok || !Storage.exists(backup.c_str())) return status;

  RsvpCheckpoint recovered;
  const auto backupStatus = readCheckpoint(backup, expectedRevision, recovered);
  if (backupStatus != CheckpointStatus::Ok) {
    return status == CheckpointStatus::Missing ? backupStatus : status;
  }
  checkpoint = recovered;
  return CheckpointStatus::Ok;
}

bool RsvpCheckpointFile::save(const std::string& cachePath, const RsvpCheckpoint& checkpoint) {
  RsvpCheckpoint existing;
  if (load(cachePath, checkpoint.bookRevision, existing) == CheckpointStatus::Ok &&
      sameCheckpoint(existing, checkpoint)) {
    return true;
  }

  std::array<uint8_t, RsvpCheckpointCodec::kEncodedSize> bytes{};
  if (!RsvpCheckpointCodec::encode(checkpoint, bytes.data(), bytes.size())) return false;

  const std::string path = checkpointPath(cachePath);
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  if (Storage.exists(temporary.c_str())) Storage.remove(temporary.c_str());

  HalFile file;
  if (!Storage.openFileForWrite("RSVP", temporary, file)) return false;
  const bool wrote = file.write(bytes.data(), bytes.size()) == bytes.size();
  file.flush();
  file.close();
  if (!wrote) {
    Storage.remove(temporary.c_str());
    return false;
  }

  if (Storage.exists(backup.c_str())) Storage.remove(backup.c_str());
  const bool hadPrevious = Storage.exists(path.c_str());
  if (hadPrevious && !Storage.rename(path.c_str(), backup.c_str())) {
    Storage.remove(temporary.c_str());
    return false;
  }
  if (!Storage.rename(temporary.c_str(), path.c_str())) {
    // A failed restore deliberately leaves .bak in place. load() promotes it
    // on the next attempt, so the last known-good checkpoint remains durable.
    const bool restored = !hadPrevious || Storage.rename(backup.c_str(), path.c_str());
    Storage.remove(temporary.c_str());
    (void)restored;
    return false;
  }
  if (hadPrevious) Storage.remove(backup.c_str());
  return true;
}

void RsvpCheckpointFile::invalidate(const std::string& cachePath) {
  const std::string path = checkpointPath(cachePath);
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  if (Storage.exists(temporary.c_str())) Storage.remove(temporary.c_str());
  if (Storage.exists(backup.c_str())) Storage.remove(backup.c_str());
}

}  // namespace rsvp

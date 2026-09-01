#include "RsvpCheckpoint.h"

#include <cstring>

namespace rsvp {

namespace {

constexpr size_t kRevisionOffset = 8;
constexpr size_t kSpineOffset = 16;
constexpr size_t kOrdinalOffset = 18;
constexpr size_t kVisibleOffset = 20;
constexpr size_t kTokenHashOffset = 24;
constexpr size_t kTokenLengthOffset = 28;
constexpr size_t kFlagsOffset = 30;
constexpr size_t kActiveTimeOffset = 32;
constexpr size_t kReservedOffset = 40;
constexpr size_t kCrcOffset = 44;

void putU16(uint8_t* bytes, const uint16_t value) {
  bytes[0] = static_cast<uint8_t>(value);
  bytes[1] = static_cast<uint8_t>(value >> 8);
}

void putU32(uint8_t* bytes, const uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value);
  bytes[1] = static_cast<uint8_t>(value >> 8);
  bytes[2] = static_cast<uint8_t>(value >> 16);
  bytes[3] = static_cast<uint8_t>(value >> 24);
}

void putU64(uint8_t* bytes, const uint64_t value) {
  putU32(bytes, static_cast<uint32_t>(value));
  putU32(bytes + 4, static_cast<uint32_t>(value >> 32));
}

uint16_t getU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1] << 8);
}

uint32_t getU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

uint64_t getU64(const uint8_t* bytes) {
  return static_cast<uint64_t>(getU32(bytes)) | (static_cast<uint64_t>(getU32(bytes + 4)) << 32);
}

uint32_t crc32(const uint8_t* bytes, const size_t length) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t index = 0; index < length; ++index) {
    crc ^= bytes[index];
    for (unsigned bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

}  // namespace

bool RsvpCheckpointCodec::encode(const RsvpCheckpoint& checkpoint, uint8_t* output, const size_t outputSize) {
  if (output == nullptr || outputSize < kEncodedSize || !checkpoint.anchor.valid || checkpoint.tokenLength == 0 ||
      checkpoint.tokenLength > MAX_TOKEN_BYTES) {
    return false;
  }

  std::memcpy(output, "RSPC", 4);
  putU16(output + 4, kFormatVersion);
  putU16(output + 6, static_cast<uint16_t>(kEncodedSize));
  putU64(output + kRevisionOffset, checkpoint.bookRevision);
  putU16(output + kSpineOffset, checkpoint.anchor.spineIndex);
  putU16(output + kOrdinalOffset, checkpoint.anchor.sameOffsetOrdinal);
  putU32(output + kVisibleOffset, checkpoint.anchor.visibleTextOffset);
  putU32(output + kTokenHashOffset, checkpoint.tokenHash32);
  putU16(output + kTokenLengthOffset, checkpoint.tokenLength);
  putU16(output + kFlagsOffset, 1);
  putU64(output + kActiveTimeOffset, checkpoint.activeRsvpTimeMs);
  putU32(output + kReservedOffset, 0);
  putU32(output + kCrcOffset, crc32(output, kCrcOffset));
  return true;
}

CheckpointStatus RsvpCheckpointCodec::decode(const uint8_t* data, const size_t dataSize,
                                             const uint64_t expectedBookRevision, RsvpCheckpoint& output) {
  if (data == nullptr || dataSize == 0) return CheckpointStatus::Missing;
  if (dataSize < kEncodedSize) return CheckpointStatus::Truncated;
  if (dataSize > kEncodedSize) return CheckpointStatus::TrailingData;
  if (std::memcmp(data, "RSPC", 4) != 0) return CheckpointStatus::Corrupt;
  if (getU16(data + 4) != kFormatVersion) return CheckpointStatus::UnsupportedVersion;
  if (getU16(data + 6) != kEncodedSize) return CheckpointStatus::Corrupt;
  if (getU32(data + kCrcOffset) != crc32(data, kCrcOffset)) return CheckpointStatus::Corrupt;
  if (getU16(data + kFlagsOffset) != 1 || getU32(data + kReservedOffset) != 0) {
    return CheckpointStatus::Corrupt;
  }
  const uint16_t tokenLength = getU16(data + kTokenLengthOffset);
  if (tokenLength == 0 || tokenLength > MAX_TOKEN_BYTES) return CheckpointStatus::Corrupt;

  const uint64_t bookRevision = getU64(data + kRevisionOffset);
  if (bookRevision != expectedBookRevision) return CheckpointStatus::RevisionMismatch;

  RsvpCheckpoint decoded;
  decoded.bookRevision = bookRevision;
  decoded.anchor.spineIndex = getU16(data + kSpineOffset);
  decoded.anchor.sameOffsetOrdinal = getU16(data + kOrdinalOffset);
  decoded.anchor.visibleTextOffset = getU32(data + kVisibleOffset);
  decoded.anchor.valid = true;
  decoded.tokenHash32 = getU32(data + kTokenHashOffset);
  decoded.tokenLength = tokenLength;
  decoded.activeRsvpTimeMs = getU64(data + kActiveTimeOffset);
  output = decoded;
  return CheckpointStatus::Ok;
}

}  // namespace rsvp

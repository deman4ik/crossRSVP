#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstdint>

#include "RsvpCheckpoint.h"

namespace {

rsvp::RsvpCheckpoint sampleCheckpoint() {
  return {
      .bookRevision = 0x1122334455667788ULL,
      .anchor = {.spineIndex = 7, .visibleTextOffset = 0xA1B2C3D4, .sameOffsetOrdinal = 3, .valid = true},
      .tokenHash32 = 0xCAFEBABEU,
      .tokenLength = 12,
      .activeRsvpTimeMs = 0x0102030405060708ULL,
  };
}

std::array<uint8_t, rsvp::RsvpCheckpointCodec::kEncodedSize> encodeSample() {
  std::array<uint8_t, rsvp::RsvpCheckpointCodec::kEncodedSize> bytes{};
  EXPECT_TRUE(rsvp::RsvpCheckpointCodec::encode(sampleCheckpoint(), bytes.data(), bytes.size()));
  return bytes;
}

TEST(RsvpCheckpoint, EncodesLittleEndianVersionAndAnchorIdentity) {
  const auto bytes = encodeSample();

  EXPECT_EQ(bytes[0], 'R');
  EXPECT_EQ(bytes[1], 'S');
  EXPECT_EQ(bytes[2], 'P');
  EXPECT_EQ(bytes[3], 'C');
  EXPECT_EQ(bytes[4], 1);
  EXPECT_EQ(bytes[5], 0);
  EXPECT_EQ(bytes[6], rsvp::RsvpCheckpointCodec::kEncodedSize & 0xFF);
  EXPECT_EQ(bytes[7], rsvp::RsvpCheckpointCodec::kEncodedSize >> 8);
  EXPECT_EQ(bytes[8], 0x88);
  EXPECT_EQ(bytes[9], 0x77);
  EXPECT_EQ(bytes[16], 7);
  EXPECT_EQ(bytes[17], 0);
  EXPECT_EQ(bytes[18], 3);
  EXPECT_EQ(bytes[19], 0);
  EXPECT_EQ(bytes[20], 0xD4);
  EXPECT_EQ(bytes[21], 0xC3);
  EXPECT_EQ(bytes[22], 0xB2);
  EXPECT_EQ(bytes[23], 0xA1);
  EXPECT_EQ(bytes[24], 0xBE);
  EXPECT_EQ(bytes[25], 0xBA);
  EXPECT_EQ(bytes[26], 0xFE);
  EXPECT_EQ(bytes[27], 0xCA);
  EXPECT_EQ(bytes[28], 12);
  EXPECT_EQ(bytes[29], 0);
  EXPECT_EQ(bytes[32], 0x08);
  EXPECT_EQ(bytes[33], 0x07);
}

TEST(RsvpCheckpoint, RoundTripsRevisionAnchorTokenIdentityAndActiveTime) {
  const auto bytes = encodeSample();
  rsvp::RsvpCheckpoint decoded;

  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(bytes.data(), bytes.size(), sampleCheckpoint().bookRevision, decoded),
            rsvp::CheckpointStatus::Ok);
  EXPECT_EQ(decoded.bookRevision, sampleCheckpoint().bookRevision);
  EXPECT_EQ(decoded.anchor.spineIndex, 7);
  EXPECT_EQ(decoded.anchor.visibleTextOffset, 0xA1B2C3D4U);
  EXPECT_EQ(decoded.anchor.sameOffsetOrdinal, 3);
  EXPECT_TRUE(decoded.anchor.valid);
  EXPECT_EQ(decoded.tokenHash32, 0xCAFEBABEU);
  EXPECT_EQ(decoded.tokenLength, 12);
  EXPECT_EQ(decoded.activeRsvpTimeMs, 0x0102030405060708ULL);
}

TEST(RsvpCheckpoint, RejectsInvalidAnchorAndSmallOutputBuffer) {
  auto checkpoint = sampleCheckpoint();
  checkpoint.anchor.valid = false;
  std::array<uint8_t, rsvp::RsvpCheckpointCodec::kEncodedSize> bytes{};
  EXPECT_FALSE(rsvp::RsvpCheckpointCodec::encode(checkpoint, bytes.data(), bytes.size()));

  checkpoint.anchor.valid = true;
  EXPECT_FALSE(rsvp::RsvpCheckpointCodec::encode(checkpoint, bytes.data(), bytes.size() - 1));
}

TEST(RsvpCheckpoint, DistinguishesMissingTruncatedAndTrailingData) {
  const auto bytes = encodeSample();
  rsvp::RsvpCheckpoint decoded;

  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(nullptr, 0, sampleCheckpoint().bookRevision, decoded),
            rsvp::CheckpointStatus::Missing);
  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(bytes.data(), bytes.size() - 1, sampleCheckpoint().bookRevision, decoded),
            rsvp::CheckpointStatus::Truncated);

  std::array<uint8_t, rsvp::RsvpCheckpointCodec::kEncodedSize + 1> trailing{};
  std::copy(bytes.begin(), bytes.end(), trailing.begin());
  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(trailing.data(), trailing.size(), sampleCheckpoint().bookRevision,
                                               decoded),
            rsvp::CheckpointStatus::TrailingData);
}

TEST(RsvpCheckpoint, RejectsUnsupportedVersionCorruptionAndWrongRevision) {
  auto bytes = encodeSample();
  rsvp::RsvpCheckpoint decoded;

  bytes[4] = 2;
  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(bytes.data(), bytes.size(), sampleCheckpoint().bookRevision, decoded),
            rsvp::CheckpointStatus::UnsupportedVersion);

  bytes = encodeSample();
  bytes[24] ^= 0x01;
  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(bytes.data(), bytes.size(), sampleCheckpoint().bookRevision, decoded),
            rsvp::CheckpointStatus::Corrupt);

  bytes = encodeSample();
  EXPECT_EQ(rsvp::RsvpCheckpointCodec::decode(bytes.data(), bytes.size(), 0x8877665544332211ULL, decoded),
            rsvp::CheckpointStatus::RevisionMismatch);
}

}  // namespace

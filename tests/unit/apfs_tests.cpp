#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "orchard/apfs/discovery.h"
#include "orchard/apfs/inspection.h"
#include "orchard/apfs/probe.h"
#include "orchard/blockio/inspection_target.h"
#include "orchard/blockio/reader.h"
#include "orchard_test/test.h"

namespace {

constexpr std::uint32_t kApfsBlockSize = 4096U;
constexpr std::uint64_t kVolumeObjectId = 77U;

void WriteLe16(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void WriteLe32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
  WriteLe16(bytes, offset, static_cast<std::uint16_t>(value & 0xFFFFU));
  WriteLe16(bytes, offset + 2U, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

void WriteLe64(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint64_t value) {
  WriteLe32(bytes, offset, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
  WriteLe32(bytes, offset + 4U, static_cast<std::uint32_t>((value >> 32U) & 0xFFFFFFFFULL));
}

void WriteAscii(std::vector<std::uint8_t>& bytes,
                const std::size_t offset,
                const std::string_view text) {
  for (std::size_t index = 0; index < text.size(); ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(text[index]);
  }
}

void WriteUtf16Le(std::vector<std::uint8_t>& bytes,
                  const std::size_t offset,
                  const std::string_view text) {
  for (std::size_t index = 0; index < text.size(); ++index) {
    WriteLe16(bytes, offset + (index * 2U), static_cast<std::uint16_t>(text[index]));
  }
}

void WriteRawUuid(std::vector<std::uint8_t>& bytes,
                  const std::size_t offset,
                  const std::array<std::uint8_t, 16>& uuid) {
  std::copy(uuid.begin(), uuid.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

void WriteNxSuperblock(std::vector<std::uint8_t>& bytes,
                       const std::size_t base_offset,
                       const std::uint64_t xid,
                       const std::uint64_t block_count) {
  WriteLe64(bytes, base_offset + 0x08U, 1U);
  WriteLe64(bytes, base_offset + 0x10U, xid);
  WriteAscii(bytes, base_offset + 0x20U, "NXSB");
  WriteLe32(bytes, base_offset + 0x24U, kApfsBlockSize);
  WriteLe64(bytes, base_offset + 0x28U, block_count);
  WriteLe64(bytes, base_offset + 0x30U, 0U);
  WriteLe64(bytes, base_offset + 0x38U, 0U);
  WriteLe64(bytes, base_offset + 0x40U, 0U);
  WriteRawUuid(bytes,
               base_offset + 0x48U,
               std::array<std::uint8_t, 16>{0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23,
                                            0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x43});
  WriteLe64(bytes, base_offset + 0x60U, xid + 1U);
  WriteLe32(bytes, base_offset + 0x68U, 1U);
  WriteLe32(bytes, base_offset + 0x6CU, 0U);
  WriteLe64(bytes, base_offset + 0x70U, 1U);
  WriteLe64(bytes, base_offset + 0x78U, 0U);
  WriteLe64(bytes, base_offset + 0x98U, 5U);
  WriteLe64(bytes, base_offset + 0xA0U, 6U);
  WriteLe64(bytes, base_offset + 0xA8U, 7U);
  WriteLe32(bytes, base_offset + 0xB4U, 100U);
  WriteLe64(bytes, base_offset + 0xB8U, kVolumeObjectId);
}

void WriteVolumeSuperblock(std::vector<std::uint8_t>& bytes,
                           const std::size_t base_offset,
                           const std::uint64_t object_id,
                           const std::uint64_t incompatible_features,
                           const std::uint16_t role,
                           const std::string_view name) {
  WriteLe64(bytes, base_offset + 0x08U, object_id);
  WriteLe64(bytes, base_offset + 0x10U, 42U);
  WriteAscii(bytes, base_offset + 0x20U, "APSB");
  WriteLe32(bytes, base_offset + 0x24U, 0U);
  WriteLe64(bytes, base_offset + 0x28U, 0U);
  WriteLe64(bytes, base_offset + 0x30U, 0U);
  WriteLe64(bytes, base_offset + 0x38U, incompatible_features);
  WriteRawUuid(bytes,
               base_offset + 0xF0U,
               std::array<std::uint8_t, 16>{0x50, 0x51, 0x52, 0x53, 0x60, 0x61, 0x62, 0x63,
                                            0x70, 0x71, 0x72, 0x73, 0x80, 0x81, 0x82, 0x83});
  WriteAscii(bytes, base_offset + 0x2C0U, name);
  WriteLe16(bytes, base_offset + 0x3C4U, role);
}

std::vector<std::uint8_t> MakeDirectFixture(const std::string_view volume_name = "Orchard Data",
                                            const std::uint64_t incompatible_features = 0x1U,
                                            const std::uint16_t role = 0x0040U) {
  std::vector<std::uint8_t> bytes(kApfsBlockSize * 8U, 0U);
  WriteNxSuperblock(bytes, 0U, 1U, 8U);
  WriteNxSuperblock(bytes, kApfsBlockSize, 42U, 8U);
  WriteVolumeSuperblock(bytes, kApfsBlockSize * 2U, kVolumeObjectId, incompatible_features, role, volume_name);
  return bytes;
}

std::vector<std::uint8_t> MakeGptFixture() {
  constexpr std::uint32_t logical_block_size = 512U;
  constexpr std::uint64_t first_lba = 40U;
  constexpr std::uint64_t last_lba = 103U;
  std::vector<std::uint8_t> bytes(logical_block_size * 256U, 0U);

  WriteAscii(bytes, logical_block_size, "EFI PART");
  WriteLe32(bytes, logical_block_size + 8U, 0x00010000U);
  WriteLe32(bytes, logical_block_size + 12U, 92U);
  WriteLe64(bytes, logical_block_size + 24U, 1U);
  WriteLe64(bytes, logical_block_size + 32U, 255U);
  WriteLe64(bytes, logical_block_size + 40U, 34U);
  WriteLe64(bytes, logical_block_size + 48U, 200U);
  WriteLe64(bytes, logical_block_size + 72U, 2U);
  WriteLe32(bytes, logical_block_size + 80U, 1U);
  WriteLe32(bytes, logical_block_size + 84U, 128U);

  const auto partition_offset = logical_block_size * 2U;
  const std::array<std::uint8_t, 16> apfs_guid{
      0xEF, 0x57, 0x34, 0x7C, 0x00, 0x00, 0xAA, 0x11,
      0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC};
  std::copy(apfs_guid.begin(), apfs_guid.end(), bytes.begin() + static_cast<std::ptrdiff_t>(partition_offset));
  WriteRawUuid(bytes,
               partition_offset + 16U,
               std::array<std::uint8_t, 16>{0x91, 0x92, 0x93, 0x94, 0xA0, 0xA1, 0xA2, 0xA3,
                                            0xB0, 0xB1, 0xB2, 0xB3, 0xC0, 0xC1, 0xC2, 0xC3});
  WriteLe64(bytes, partition_offset + 32U, first_lba);
  WriteLe64(bytes, partition_offset + 40U, last_lba);
  WriteUtf16Le(bytes, partition_offset + 56U, "Orchard GPT");

  const auto container_offset = static_cast<std::size_t>(first_lba * logical_block_size);
  WriteNxSuperblock(bytes, container_offset, 1U, 8U);
  WriteNxSuperblock(bytes, container_offset + kApfsBlockSize, 42U, 8U);
  WriteVolumeSuperblock(bytes,
                        container_offset + (kApfsBlockSize * 2U),
                        kVolumeObjectId,
                        0x1U,
                        0x0040U,
                        "GPT Data");
  return bytes;
}

void DetectsNxsbMagicAtObjectOffset() {
  std::vector<std::uint8_t> bytes(64U, 0U);
  WriteAscii(bytes, orchard::apfs::kApfsObjectMagicOffset, "NXSB");

  ORCHARD_TEST_REQUIRE(orchard::apfs::ProbeContainerMagic(bytes));
  ORCHARD_TEST_REQUIRE(!orchard::apfs::ProbeVolumeMagic(bytes));
}

void DiscoversDirectContainerAndCheckpoint() {
  auto reader = orchard::blockio::MakeMemoryReader(MakeDirectFixture(), "direct-fixture");
  const auto result = orchard::apfs::Discover(*reader);

  ORCHARD_TEST_REQUIRE(result.ok());
  ORCHARD_TEST_REQUIRE(result.value().layout == orchard::apfs::LayoutKind::kDirectContainer);
  ORCHARD_TEST_REQUIRE(result.value().containers.size() == 1U);

  const auto& container = result.value().containers[0];
  ORCHARD_TEST_REQUIRE(container.block_size == kApfsBlockSize);
  ORCHARD_TEST_REQUIRE(container.selected_checkpoint.xid == 42U);
  ORCHARD_TEST_REQUIRE(container.selected_checkpoint.source ==
                       orchard::apfs::CheckpointSource::kCheckpointDescriptorArea);
  ORCHARD_TEST_REQUIRE(container.volume_object_ids.size() == 1U);
  ORCHARD_TEST_REQUIRE(container.volumes.size() == 1U);
  ORCHARD_TEST_REQUIRE(container.volumes[0].name == "Orchard Data");
  ORCHARD_TEST_REQUIRE(container.volumes[0].role_names.size() == 1U);
  ORCHARD_TEST_REQUIRE(container.volumes[0].role_names[0] == "data");
  ORCHARD_TEST_REQUIRE(container.volumes[0].case_insensitive);
}

void DiscoversGptWrappedContainer() {
  auto reader = orchard::blockio::MakeMemoryReader(MakeGptFixture(), "gpt-fixture");
  const auto result = orchard::apfs::Discover(*reader);

  ORCHARD_TEST_REQUIRE(result.ok());
  ORCHARD_TEST_REQUIRE(result.value().layout == orchard::apfs::LayoutKind::kGuidPartitionTable);
  ORCHARD_TEST_REQUIRE(result.value().gpt_block_size.has_value());
  ORCHARD_TEST_REQUIRE(result.value().gpt_block_size.value() == 512U);
  ORCHARD_TEST_REQUIRE(result.value().partitions.size() == 1U);
  ORCHARD_TEST_REQUIRE(result.value().partitions[0].is_apfs_partition);
  ORCHARD_TEST_REQUIRE(result.value().containers.size() == 1U);
  ORCHARD_TEST_REQUIRE(result.value().containers[0].partition.has_value());
  ORCHARD_TEST_REQUIRE(result.value().containers[0].partition->name == "Orchard GPT");
  ORCHARD_TEST_REQUIRE(result.value().containers[0].volumes.size() == 1U);
  ORCHARD_TEST_REQUIRE(result.value().containers[0].volumes[0].name == "GPT Data");
}

void InspectTargetUsesRealReaderPath() {
  const auto temp_path = std::filesystem::temp_directory_path() / "orchard_apfs_direct.img";
  const auto bytes = MakeDirectFixture();

  {
    std::ofstream output(temp_path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }

  const auto target_info = orchard::blockio::InspectTargetPath(temp_path);
  const auto result = orchard::apfs::InspectTarget(target_info);

  ORCHARD_TEST_REQUIRE(result.status == orchard::apfs::InspectionStatus::kSuccess);
  ORCHARD_TEST_REQUIRE(result.report.layout == orchard::apfs::LayoutKind::kDirectContainer);
  ORCHARD_TEST_REQUIRE(result.report.containers.size() == 1U);
  ORCHARD_TEST_REQUIRE(result.report.containers[0].volumes.size() == 1U);

  std::filesystem::remove(temp_path);
}

} // namespace

int main() {
  return orchard_test::RunTests({
      {"DetectsNxsbMagicAtObjectOffset", &DetectsNxsbMagicAtObjectOffset},
      {"DiscoversDirectContainerAndCheckpoint", &DiscoversDirectContainerAndCheckpoint},
      {"DiscoversGptWrappedContainer", &DiscoversGptWrappedContainer},
      {"InspectTargetUsesRealReaderPath", &InspectTargetUsesRealReaderPath},
  });
}

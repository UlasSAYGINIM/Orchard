#include "orchard/apfs/discovery.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

#include "orchard/apfs/probe.h"

namespace orchard::apfs {
namespace {

constexpr std::array<std::uint8_t, 8> kGptSignature{
    static_cast<std::uint8_t>('E'),
    static_cast<std::uint8_t>('F'),
    static_cast<std::uint8_t>('I'),
    static_cast<std::uint8_t>(' '),
    static_cast<std::uint8_t>('P'),
    static_cast<std::uint8_t>('A'),
    static_cast<std::uint8_t>('R'),
    static_cast<std::uint8_t>('T'),
};

constexpr std::array<std::uint8_t, 16> kApfsPartitionTypeGuid{
    0xEF,
    0x57,
    0x34,
    0x7C,
    0x00,
    0x00,
    0xAA,
    0x11,
    0xAA,
    0x11,
    0x00,
    0x30,
    0x65,
    0x43,
    0xEC,
    0xAC,
};

constexpr std::uint32_t kApfsMinimumBlockSize = 4096;
constexpr std::uint32_t kApfsMaximumBlockSize = 65536;
constexpr std::uint32_t kGptMinimumEntrySize = 128;
constexpr std::uint32_t kGptMaximumEntryCount = 256;
constexpr std::uint64_t kMaximumGptEntryTableBytes = 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumCheckpointScanBlocks = 2048ULL;
constexpr std::uint64_t kMaximumFallbackVolumeScanBytes = 128ULL * 1024ULL * 1024ULL;

constexpr std::uint64_t kNxIncompatFusion = 0x100ULL;

constexpr std::uint64_t kVolumeIncompatCaseInsensitive = 0x1ULL;
constexpr std::uint64_t kVolumeIncompatDatalessSnaps = 0x2ULL;
constexpr std::uint64_t kVolumeIncompatEncRolled = 0x4ULL;
constexpr std::uint64_t kVolumeIncompatNormalizationInsensitive = 0x8ULL;
constexpr std::uint64_t kVolumeIncompatIncompleteRestore = 0x10ULL;
constexpr std::uint64_t kVolumeIncompatSealed = 0x20ULL;

constexpr std::uint16_t kRoleSystem = 0x0001U;
constexpr std::uint16_t kRoleUser = 0x0002U;
constexpr std::uint16_t kRoleRecovery = 0x0004U;
constexpr std::uint16_t kRoleVm = 0x0008U;
constexpr std::uint16_t kRolePreboot = 0x0010U;
constexpr std::uint16_t kRoleInstaller = 0x0020U;
constexpr std::uint16_t kRoleData = 0x0040U;

struct GptHeader {
  std::uint32_t logical_block_size = 0;
  std::uint64_t partition_entries_lba = 0;
  std::uint32_t partition_entry_count = 0;
  std::uint32_t partition_entry_size = 0;
};

struct ParsedNxSuperblock {
  FeatureFlags features;
  std::uint32_t block_size = 0;
  std::uint64_t block_count = 0;
  std::string uuid;
  std::uint64_t xid = 0;
  std::uint64_t next_xid = 0;
  std::uint32_t checkpoint_descriptor_blocks = 0;
  std::uint64_t checkpoint_descriptor_base = 0;
  std::uint64_t spaceman_oid = 0;
  std::uint64_t omap_oid = 0;
  std::uint64_t reaper_oid = 0;
  std::vector<std::uint64_t> volume_object_ids;
};

struct ParsedVolumeSuperblock {
  VolumeInfo volume;
};

struct CandidateContainer {
  ParsedNxSuperblock superblock;
  std::uint64_t block_index = 0;
  CheckpointSource source = CheckpointSource::kMainSuperblock;
};

[[nodiscard]] blockio::Error MakeError(const blockio::ErrorCode code, std::string message) {
  return blockio::Error{
      .code = code,
      .message = std::move(message),
  };
}

[[nodiscard]] bool HasRange(const std::span<const std::uint8_t> bytes,
                            const std::size_t offset,
                            const std::size_t length) noexcept {
  return offset <= bytes.size() && length <= bytes.size() - offset;
}

[[nodiscard]] std::uint16_t ReadLe16(const std::span<const std::uint8_t> bytes,
                                     const std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(bytes[offset]) |
         (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t ReadLe32(const std::span<const std::uint8_t> bytes,
                                     const std::size_t offset) noexcept {
  return static_cast<std::uint32_t>(ReadLe16(bytes, offset)) |
         (static_cast<std::uint32_t>(ReadLe16(bytes, offset + 2U)) << 16U);
}

[[nodiscard]] std::uint64_t ReadLe64(const std::span<const std::uint8_t> bytes,
                                     const std::size_t offset) noexcept {
  return static_cast<std::uint64_t>(ReadLe32(bytes, offset)) |
         (static_cast<std::uint64_t>(ReadLe32(bytes, offset + 4U)) << 32U);
}

[[nodiscard]] std::string FormatRawUuid(const std::span<const std::uint8_t> bytes) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";

  std::string text;
  text.reserve(36);

  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U) {
      text.push_back('-');
    }

    text.push_back(kHexDigits[(bytes[index] >> 4U) & 0x0FU]);
    text.push_back(kHexDigits[bytes[index] & 0x0FU]);
  }

  return text;
}

[[nodiscard]] std::string FormatGuidFromGptBytes(const std::span<const std::uint8_t, 16> bytes) {
  std::array<std::uint8_t, 16> canonical{};
  canonical[0] = bytes[3];
  canonical[1] = bytes[2];
  canonical[2] = bytes[1];
  canonical[3] = bytes[0];
  canonical[4] = bytes[5];
  canonical[5] = bytes[4];
  canonical[6] = bytes[7];
  canonical[7] = bytes[6];
  std::copy(bytes.begin() + 8, bytes.end(), canonical.begin() + 8);
  return FormatRawUuid(canonical);
}

[[nodiscard]] std::optional<std::uint64_t> TryGetReaderSize(const blockio::Reader& reader) {
  auto size_result = reader.size_bytes();
  if (!size_result.ok()) {
    return std::nullopt;
  }

  return size_result.value();
}

[[nodiscard]] bool IsReasonableApfsBlockSize(const std::uint32_t block_size) noexcept {
  if (block_size < kApfsMinimumBlockSize || block_size > kApfsMaximumBlockSize) {
    return false;
  }

  return (block_size & (block_size - 1U)) == 0U;
}

[[nodiscard]] std::string DecodeUtf16LeName(const std::span<const std::uint8_t> bytes) {
  std::string name;
  name.reserve(bytes.size() / 2U);

  for (std::size_t offset = 0; offset + 1U < bytes.size(); offset += 2U) {
    const auto code_unit = ReadLe16(bytes, offset);
    if (code_unit == 0U) {
      break;
    }

    if (code_unit <= 0x7FU) {
      name.push_back(static_cast<char>(code_unit));
    } else {
      name.push_back('?');
    }
  }

  return name;
}

[[nodiscard]] std::string DecodeUtf8Name(const std::span<const std::uint8_t> bytes) {
  auto end = std::find(bytes.begin(), bytes.end(), static_cast<std::uint8_t>(0));
  return std::string(bytes.begin(), end);
}

FeatureFlags MakeContainerFeatures(const std::uint64_t compatible,
                                   const std::uint64_t readonly_compatible,
                                   const std::uint64_t incompatible) {
  FeatureFlags features;
  features.compatible = compatible;
  features.readonly_compatible = readonly_compatible;
  features.incompatible = incompatible;

  if ((incompatible & kNxIncompatFusion) != 0U) {
    features.incompatible_names.emplace_back("fusion");
  }

  return features;
}

FeatureFlags MakeVolumeFeatures(const std::uint64_t compatible,
                                const std::uint64_t readonly_compatible,
                                const std::uint64_t incompatible) {
  FeatureFlags features;
  features.compatible = compatible;
  features.readonly_compatible = readonly_compatible;
  features.incompatible = incompatible;

  if ((incompatible & kVolumeIncompatCaseInsensitive) != 0U) {
    features.incompatible_names.emplace_back("case_insensitive");
  }
  if ((incompatible & kVolumeIncompatDatalessSnaps) != 0U) {
    features.incompatible_names.emplace_back("dataless_snapshots");
  }
  if ((incompatible & kVolumeIncompatEncRolled) != 0U) {
    features.incompatible_names.emplace_back("encryption_rolled");
  }
  if ((incompatible & kVolumeIncompatNormalizationInsensitive) != 0U) {
    features.incompatible_names.emplace_back("normalization_insensitive");
  }
  if ((incompatible & kVolumeIncompatIncompleteRestore) != 0U) {
    features.incompatible_names.emplace_back("incomplete_restore");
  }
  if ((incompatible & kVolumeIncompatSealed) != 0U) {
    features.incompatible_names.emplace_back("sealed");
  }

  return features;
}

[[nodiscard]] std::vector<std::string> DecodeVolumeRoles(const std::uint16_t role) {
  std::vector<std::string> roles;

  if ((role & kRoleSystem) != 0U) {
    roles.emplace_back("system");
  }
  if ((role & kRoleUser) != 0U) {
    roles.emplace_back("user");
  }
  if ((role & kRoleRecovery) != 0U) {
    roles.emplace_back("recovery");
  }
  if ((role & kRoleVm) != 0U) {
    roles.emplace_back("vm");
  }
  if ((role & kRolePreboot) != 0U) {
    roles.emplace_back("preboot");
  }
  if ((role & kRoleInstaller) != 0U) {
    roles.emplace_back("installer");
  }
  if ((role & kRoleData) != 0U) {
    roles.emplace_back("data");
  }

  return roles;
}

blockio::Result<ParsedNxSuperblock> ParseNxSuperblock(const std::span<const std::uint8_t> block) {
  if (!HasRange(block, 0x20U, 4U) || !ProbeContainerMagic(block)) {
    return MakeError(blockio::ErrorCode::kInvalidFormat,
                     "Container superblock magic is missing at the APFS object offset.");
  }

  if (!HasRange(block, 0x3E8U, 0x10U)) {
    return MakeError(blockio::ErrorCode::kShortRead,
                     "Container superblock block is too small for required fields.");
  }

  const auto block_size = ReadLe32(block, 0x24U);
  if (!IsReasonableApfsBlockSize(block_size)) {
    return MakeError(blockio::ErrorCode::kCorruptData,
                     "Container superblock advertises an invalid APFS block size.");
  }

  ParsedNxSuperblock parsed;
  parsed.features = MakeContainerFeatures(ReadLe64(block, 0x30U),
                                          ReadLe64(block, 0x38U),
                                          ReadLe64(block, 0x40U));
  parsed.block_size = block_size;
  parsed.block_count = ReadLe64(block, 0x28U);
  parsed.uuid = FormatRawUuid(std::span(block.begin() + 0x48U, 16U));
  parsed.xid = ReadLe64(block, 0x10U);
  parsed.next_xid = ReadLe64(block, 0x60U);
  parsed.checkpoint_descriptor_blocks = ReadLe32(block, 0x68U);
  parsed.checkpoint_descriptor_base = ReadLe64(block, 0x70U);
  parsed.spaceman_oid = ReadLe64(block, 0x98U);
  parsed.omap_oid = ReadLe64(block, 0xA0U);
  parsed.reaper_oid = ReadLe64(block, 0xA8U);

  const auto max_file_systems = std::min<std::uint32_t>(ReadLe32(block, 0xB4U), 100U);
  for (std::uint32_t index = 0; index < max_file_systems; ++index) {
    const auto entry_offset = static_cast<std::size_t>(0xB8U + (index * 8U));
    if (!HasRange(block, entry_offset, 8U)) {
      break;
    }

    const auto object_id = ReadLe64(block, entry_offset);
    if (object_id != 0U) {
      parsed.volume_object_ids.push_back(object_id);
    }
  }

  return parsed;
}

blockio::Result<ParsedVolumeSuperblock> ParseVolumeSuperblock(
    const std::span<const std::uint8_t> block) {
  if (!HasRange(block, 0x400U, 8U) || !ProbeVolumeMagic(block)) {
    return MakeError(blockio::ErrorCode::kInvalidFormat,
                     "Volume superblock magic is missing at the APFS object offset.");
  }

  ParsedVolumeSuperblock parsed;
  parsed.volume.object_id = ReadLe64(block, 0x08U);
  parsed.volume.filesystem_index = ReadLe32(block, 0x24U);
  parsed.volume.features = MakeVolumeFeatures(ReadLe64(block, 0x28U),
                                              ReadLe64(block, 0x30U),
                                              ReadLe64(block, 0x38U));
  parsed.volume.uuid = FormatRawUuid(std::span(block.begin() + 0xF0U, 16U));
  parsed.volume.name = DecodeUtf8Name(std::span(block.begin() + 0x2C0U, 256U));
  parsed.volume.role = ReadLe16(block, 0x3C4U);
  parsed.volume.role_names = DecodeVolumeRoles(parsed.volume.role);
  parsed.volume.case_insensitive =
      (parsed.volume.features.incompatible & kVolumeIncompatCaseInsensitive) != 0U;
  parsed.volume.sealed = (parsed.volume.features.incompatible & kVolumeIncompatSealed) != 0U;
  return parsed;
}

blockio::Result<std::optional<GptHeader>> TryReadGptHeader(const blockio::Reader& reader,
                                                           const std::uint32_t logical_block_size) {
  const auto reader_size = TryGetReaderSize(reader);
  const auto header_offset = static_cast<std::uint64_t>(logical_block_size);
  if (reader_size.has_value() && *reader_size < header_offset + 92U) {
    return std::optional<GptHeader>{};
  }

  auto header_bytes_result = blockio::ReadExact(reader, header_offset, logical_block_size);
  if (!header_bytes_result.ok()) {
    if (header_bytes_result.error().code == blockio::ErrorCode::kShortRead) {
      return std::optional<GptHeader>{};
    }
    return header_bytes_result.error();
  }

  const auto& header_bytes = header_bytes_result.value();
  if (!std::equal(kGptSignature.begin(),
                  kGptSignature.end(),
                  header_bytes.begin(),
                  header_bytes.begin() + static_cast<std::ptrdiff_t>(kGptSignature.size()))) {
    return std::optional<GptHeader>{};
  }

  const auto header_size = ReadLe32(header_bytes, 12U);
  if (header_size < 92U || header_size > logical_block_size) {
    return MakeError(blockio::ErrorCode::kCorruptData,
                     "GPT header size is outside the supported range.");
  }

  const auto partition_entry_count = ReadLe32(header_bytes, 80U);
  const auto partition_entry_size = ReadLe32(header_bytes, 84U);
  if (partition_entry_size < kGptMinimumEntrySize ||
      partition_entry_count > kGptMaximumEntryCount) {
    return MakeError(blockio::ErrorCode::kCorruptData,
                     "GPT header advertises unsupported partition-entry metadata.");
  }

  const auto entry_table_bytes =
      static_cast<std::uint64_t>(partition_entry_count) * partition_entry_size;
  if (entry_table_bytes > kMaximumGptEntryTableBytes) {
    return MakeError(blockio::ErrorCode::kCorruptData,
                     "GPT partition-entry table exceeds the supported scan budget.");
  }

  return std::optional<GptHeader>(GptHeader{
      .logical_block_size = logical_block_size,
      .partition_entries_lba = ReadLe64(header_bytes, 72U),
      .partition_entry_count = partition_entry_count,
      .partition_entry_size = partition_entry_size,
  });
}

blockio::Result<std::vector<PartitionInfo>> ReadGptPartitions(const blockio::Reader& reader,
                                                              const GptHeader& header) {
  const auto table_offset = header.partition_entries_lba * header.logical_block_size;
  const auto table_bytes = static_cast<std::size_t>(
      static_cast<std::uint64_t>(header.partition_entry_count) * header.partition_entry_size);
  auto entries_result = blockio::ReadExact(reader, table_offset, table_bytes);
  if (!entries_result.ok()) {
    return entries_result.error();
  }

  std::vector<PartitionInfo> partitions;
  partitions.reserve(header.partition_entry_count);

  const auto& entries = entries_result.value();
  for (std::uint32_t index = 0; index < header.partition_entry_count; ++index) {
    const auto entry_offset = static_cast<std::size_t>(index) * header.partition_entry_size;
    const auto entry =
        std::span(entries.begin() + static_cast<std::ptrdiff_t>(entry_offset),
                  header.partition_entry_size);

    if (std::all_of(entry.begin(), entry.begin() + 16, [](const std::uint8_t value) {
          return value == 0U;
        })) {
      continue;
    }

    std::array<std::uint8_t, 16> type_guid_bytes{};
    std::copy_n(entry.begin(), 16, type_guid_bytes.begin());

    std::array<std::uint8_t, 16> unique_guid_bytes{};
    std::copy_n(entry.begin() + 16, 16, unique_guid_bytes.begin());

    const auto first_lba = ReadLe64(entry, 32U);
    const auto last_lba = ReadLe64(entry, 40U);
    if (last_lba < first_lba) {
      return MakeError(blockio::ErrorCode::kCorruptData,
                       "GPT partition has an invalid LBA range.");
    }

    partitions.push_back(PartitionInfo{
        .type_guid = FormatGuidFromGptBytes(type_guid_bytes),
        .unique_guid = FormatGuidFromGptBytes(unique_guid_bytes),
        .name = DecodeUtf16LeName(std::span(entry.begin() + 56, 72U)),
        .first_lba = first_lba,
        .last_lba = last_lba,
        .byte_offset = first_lba * header.logical_block_size,
        .byte_length = ((last_lba - first_lba) + 1U) * header.logical_block_size,
        .is_apfs_partition = std::equal(type_guid_bytes.begin(),
                                        type_guid_bytes.end(),
                                        kApfsPartitionTypeGuid.begin()),
    });
  }

  return partitions;
}

blockio::Result<std::optional<ContainerInfo>> TryReadContainerAt(
    const blockio::Reader& reader,
    const std::uint64_t byte_offset,
    const LayoutKind source_layout,
    const std::optional<PartitionInfo>& partition) {
  const auto reader_size = TryGetReaderSize(reader);
  if (reader_size.has_value() && *reader_size < byte_offset + kApfsMinimumBlockSize) {
    return std::optional<ContainerInfo>{};
  }

  auto initial_block_result = blockio::ReadExact(reader, byte_offset, kApfsMinimumBlockSize);
  if (!initial_block_result.ok()) {
    if (initial_block_result.error().code == blockio::ErrorCode::kShortRead) {
      return std::optional<ContainerInfo>{};
    }
    return initial_block_result.error();
  }

  if (!ProbeContainerMagic(initial_block_result.value())) {
    return std::optional<ContainerInfo>{};
  }

  auto initial_parse_result = ParseNxSuperblock(initial_block_result.value());
  if (!initial_parse_result.ok()) {
    return initial_parse_result.error();
  }

  const auto& initial_superblock = initial_parse_result.value();
  const auto block_size = initial_superblock.block_size;
  auto selected_candidate = CandidateContainer{
      .superblock = initial_superblock,
      .block_index = 0,
      .source = CheckpointSource::kMainSuperblock,
  };

  const auto max_candidate_blocks =
      std::min<std::uint64_t>(initial_superblock.checkpoint_descriptor_blocks,
                              kMaximumCheckpointScanBlocks);

  for (std::uint64_t block_index = 0; block_index < max_candidate_blocks; ++block_index) {
    const auto candidate_block = initial_superblock.checkpoint_descriptor_base + block_index;
    const auto candidate_offset = byte_offset + (candidate_block * block_size);
    auto candidate_bytes_result = blockio::ReadExact(reader, candidate_offset, block_size);
    if (!candidate_bytes_result.ok()) {
      continue;
    }

    if (!ProbeContainerMagic(candidate_bytes_result.value())) {
      continue;
    }

    auto candidate_parse_result = ParseNxSuperblock(candidate_bytes_result.value());
    if (!candidate_parse_result.ok()) {
      continue;
    }

    const auto& candidate = candidate_parse_result.value();
    if (candidate.xid > selected_candidate.superblock.xid) {
      selected_candidate = CandidateContainer{
          .superblock = candidate,
          .block_index = candidate_block,
          .source = CheckpointSource::kCheckpointDescriptorArea,
      };
    }
  }

  ContainerInfo container;
  container.source_layout = source_layout;
  container.byte_offset = byte_offset;
  container.byte_length =
      static_cast<std::uint64_t>(selected_candidate.superblock.block_size) *
      selected_candidate.superblock.block_count;
  container.block_size = selected_candidate.superblock.block_size;
  container.block_count = selected_candidate.superblock.block_count;
  container.uuid = selected_candidate.superblock.uuid;
  container.features = selected_candidate.superblock.features;
  container.selected_checkpoint.block_index = selected_candidate.block_index;
  container.selected_checkpoint.xid = selected_candidate.superblock.xid;
  container.selected_checkpoint.source = selected_candidate.source;
  container.spaceman_oid = selected_candidate.superblock.spaceman_oid;
  container.omap_oid = selected_candidate.superblock.omap_oid;
  container.reaper_oid = selected_candidate.superblock.reaper_oid;
  container.volume_object_ids = selected_candidate.superblock.volume_object_ids;
  container.partition = partition;

  const auto available_blocks = [&]() -> std::uint64_t {
    if (!reader_size.has_value() || *reader_size < byte_offset || block_size == 0U) {
      return container.block_count;
    }

    return std::min<std::uint64_t>(container.block_count,
                                   (*reader_size - byte_offset) / block_size);
  }();

  const auto fallback_scan_blocks = [&]() -> std::uint64_t {
    const auto byte_limited =
        std::max<std::uint64_t>(1U, kMaximumFallbackVolumeScanBytes / block_size);
    return std::min<std::uint64_t>(available_blocks, byte_limited);
  }();

  if (fallback_scan_blocks < container.block_count) {
    std::ostringstream note;
    note << "Volume enumeration is using a bounded block scan over the first "
         << fallback_scan_blocks
         << " block(s) until object-map traversal lands in M1-T04.";
    container.notes.push_back(note.str());
  } else {
    container.notes.push_back(
        "Volume enumeration currently uses a fallback block scan until object-map traversal lands in M1-T04.");
  }

  std::unordered_set<std::uint64_t> remaining_ids(container.volume_object_ids.begin(),
                                                  container.volume_object_ids.end());
  for (std::uint64_t block_index = 0; block_index < fallback_scan_blocks && !remaining_ids.empty();
       ++block_index) {
    const auto candidate_offset = byte_offset + (block_index * block_size);
    auto candidate_bytes_result = blockio::ReadExact(reader, candidate_offset, block_size);
    if (!candidate_bytes_result.ok()) {
      continue;
    }

    if (!ProbeVolumeMagic(candidate_bytes_result.value())) {
      continue;
    }

    auto volume_parse_result = ParseVolumeSuperblock(candidate_bytes_result.value());
    if (!volume_parse_result.ok()) {
      continue;
    }

    auto volume = std::move(volume_parse_result.value().volume);
    if (!remaining_ids.contains(volume.object_id)) {
      continue;
    }

    remaining_ids.erase(volume.object_id);
    container.volumes.push_back(std::move(volume));
  }

  if (!remaining_ids.empty()) {
    std::ostringstream note;
    note << "Failed to resolve " << remaining_ids.size()
         << " referenced volume superblock(s) during fallback scanning.";
    container.notes.push_back(note.str());
  }

  return std::optional<ContainerInfo>(std::move(container));
}

} // namespace

blockio::Result<DiscoveryReport> Discover(const blockio::Reader& reader) {
  DiscoveryReport report;

  auto direct_container_result =
      TryReadContainerAt(reader, 0U, LayoutKind::kDirectContainer, std::nullopt);
  if (!direct_container_result.ok()) {
    return direct_container_result.error();
  }

  if (direct_container_result.value().has_value()) {
    report.layout = LayoutKind::kDirectContainer;
    report.containers.push_back(std::move(*direct_container_result.value()));
    report.notes.push_back("Detected a direct APFS container image at byte offset 0.");
    return report;
  }

  for (const auto logical_block_size : {512U, 4096U}) {
    auto header_result = TryReadGptHeader(reader, logical_block_size);
    if (!header_result.ok()) {
      return header_result.error();
    }

    if (!header_result.value().has_value()) {
      continue;
    }

    const auto& header = *header_result.value();
    auto partitions_result = ReadGptPartitions(reader, header);
    if (!partitions_result.ok()) {
      return partitions_result.error();
    }

    report.layout = LayoutKind::kGuidPartitionTable;
    report.gpt_block_size = header.logical_block_size;
    report.partitions = partitions_result.value();

    for (const auto& partition : report.partitions) {
      if (!partition.is_apfs_partition) {
        continue;
      }

      auto container_result = TryReadContainerAt(reader,
                                                 partition.byte_offset,
                                                 LayoutKind::kGuidPartitionTable,
                                                 partition);
      if (!container_result.ok()) {
        return container_result.error();
      }

      if (container_result.value().has_value()) {
        report.containers.push_back(std::move(*container_result.value()));
      }
    }

    if (report.containers.empty()) {
      report.notes.push_back(
          "GPT was detected, but no APFS container superblock was found in APFS-typed partitions.");
    } else {
      report.notes.push_back(
          "Detected APFS container(s) inside a GPT-partitioned disk image.");
    }
    return report;
  }

  report.notes.push_back(
      "No direct APFS container or supported GPT-partitioned APFS container was detected.");
  return report;
}

std::string_view ToString(const LayoutKind layout) noexcept {
  switch (layout) {
  case LayoutKind::kUnknown:
    return "unknown";
  case LayoutKind::kDirectContainer:
    return "direct_container";
  case LayoutKind::kGuidPartitionTable:
    return "guid_partition_table";
  }

  return "unknown";
}

std::string_view ToString(const CheckpointSource source) noexcept {
  switch (source) {
  case CheckpointSource::kMainSuperblock:
    return "main_superblock";
  case CheckpointSource::kCheckpointDescriptorArea:
    return "checkpoint_descriptor_area";
  }

  return "main_superblock";
}

} // namespace orchard::apfs

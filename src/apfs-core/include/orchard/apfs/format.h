#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "orchard/blockio/result.h"

namespace orchard::apfs {

constexpr std::uint32_t kApfsMinimumBlockSize = 4096U;
constexpr std::uint32_t kApfsMaximumBlockSize = 65536U;
constexpr std::uint64_t kApfsRootDirectoryObjectId = 2U;

constexpr std::uint32_t kObjectTypeMask = 0x0000FFFFU;
constexpr std::uint32_t kObjectTypeNxSuperblock = 0x00000001U;
constexpr std::uint32_t kObjectTypeBtree = 0x00000002U;
constexpr std::uint32_t kObjectTypeBtreeNode = 0x00000003U;
constexpr std::uint32_t kObjectTypeOmap = 0x0000000BU;
constexpr std::uint32_t kObjectTypeFs = 0x0000000DU;

constexpr std::size_t kApfsObjectHeaderSize = 0x20U;
constexpr std::size_t kBtreeNodeHeaderSize = 0x38U;
constexpr std::size_t kBtreeInfoSize = 0x28U;

constexpr std::uint16_t kBtreeNodeFlagRoot = 0x0001U;
constexpr std::uint16_t kBtreeNodeFlagLeaf = 0x0002U;
constexpr std::uint16_t kBtreeNodeFlagFixedKv = 0x0004U;
constexpr std::uint16_t kBtreeNodeFlagCheckKoffInvalid = 0x8000U;
constexpr std::uint16_t kBtreeNodeFlagMask = 0x8007U;
constexpr std::uint16_t kBtreeOffsetInvalid = 0xFFFFU;

constexpr std::uint32_t kOmapValueDeleted = 0x00000001U;

constexpr std::uint64_t kNxIncompatFusion = 0x100ULL;

constexpr std::uint64_t kVolumeIncompatCaseInsensitive = 0x1ULL;
constexpr std::uint64_t kVolumeIncompatDatalessSnaps = 0x2ULL;
constexpr std::uint64_t kVolumeIncompatEncRolled = 0x4ULL;
constexpr std::uint64_t kVolumeIncompatNormalizationInsensitive = 0x8ULL;
constexpr std::uint64_t kVolumeIncompatIncompleteRestore = 0x10ULL;
constexpr std::uint64_t kVolumeIncompatSealed = 0x20ULL;

constexpr std::uint16_t kVolumeRoleSystem = 0x0001U;
constexpr std::uint16_t kVolumeRoleUser = 0x0002U;
constexpr std::uint16_t kVolumeRoleRecovery = 0x0004U;
constexpr std::uint16_t kVolumeRoleVm = 0x0008U;
constexpr std::uint16_t kVolumeRolePreboot = 0x0010U;
constexpr std::uint16_t kVolumeRoleInstaller = 0x0020U;
constexpr std::uint16_t kVolumeRoleData = 0x0040U;

struct FeatureFlags {
  std::uint64_t compatible = 0;
  std::uint64_t readonly_compatible = 0;
  std::uint64_t incompatible = 0;
  std::vector<std::string> compatible_names;
  std::vector<std::string> readonly_compatible_names;
  std::vector<std::string> incompatible_names;
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
  std::uint64_t object_id = 0;
  std::uint64_t xid = 0;
  std::uint32_t filesystem_index = 0;
  std::string name;
  std::string uuid;
  FeatureFlags features;
  std::uint16_t role = 0;
  std::vector<std::string> role_names;
  std::uint32_t root_tree_type = 0;
  std::uint64_t omap_oid = 0;
  std::uint64_t root_tree_oid = 0;
  std::uint64_t extentref_tree_oid = 0;
  std::uint64_t fext_tree_oid = 0;
  std::uint64_t doc_id_tree_oid = 0;
  std::uint64_t security_tree_oid = 0;
  std::uint64_t root_directory_object_id = kApfsRootDirectoryObjectId;
  bool case_insensitive = false;
  bool snapshots_present = false;
  bool encryption_rolled = false;
  bool incomplete_restore = false;
  bool normalization_insensitive = false;
  bool sealed = false;
};

[[nodiscard]] blockio::Error MakeApfsError(blockio::ErrorCode code, std::string message);

[[nodiscard]] bool HasRange(std::span<const std::uint8_t> bytes, std::size_t offset,
                            std::size_t length) noexcept;
[[nodiscard]] std::uint16_t ReadLe16(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) noexcept;
[[nodiscard]] std::uint32_t ReadLe32(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) noexcept;
[[nodiscard]] std::uint64_t ReadLe64(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) noexcept;

[[nodiscard]] std::string FormatRawUuid(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string FormatGuidFromGptBytes(std::span<const std::uint8_t, 16> bytes);
[[nodiscard]] std::string DecodeUtf16LeName(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string DecodeUtf8Name(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool IsReasonableApfsBlockSize(std::uint32_t block_size) noexcept;

[[nodiscard]] FeatureFlags MakeContainerFeatures(std::uint64_t compatible,
                                                 std::uint64_t readonly_compatible,
                                                 std::uint64_t incompatible);
[[nodiscard]] FeatureFlags MakeVolumeFeatures(std::uint64_t compatible,
                                              std::uint64_t readonly_compatible,
                                              std::uint64_t incompatible);
[[nodiscard]] std::vector<std::string> DecodeVolumeRoles(std::uint16_t role);

blockio::Result<ParsedNxSuperblock> ParseNxSuperblock(std::span<const std::uint8_t> block);
blockio::Result<ParsedVolumeSuperblock> ParseVolumeSuperblock(std::span<const std::uint8_t> block);

} // namespace orchard::apfs

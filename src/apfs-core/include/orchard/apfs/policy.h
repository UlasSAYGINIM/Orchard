#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace orchard::apfs {

enum class MountDisposition {
  kHide,
  kMountReadOnly,
  kMountReadWrite,
  kReject,
};

enum class PolicyReason {
  kRoleSystem,
  kRoleRecovery,
  kRoleVm,
  kRolePreboot,
  kRoleInstaller,
  kSealed,
  kSnapshotsPresent,
  kCaseSensitive,
  kContainerFusion,
  kEncryptionRolled,
  kIncompleteRestore,
  kNormalizationInsensitive,
  kUnsupportedVolumeFeature,
};

struct PolicyInput {
  std::uint16_t role = 0;
  bool container_fusion = false;
  bool case_insensitive = false;
  bool sealed = false;
  bool snapshots_present = false;
  bool encryption_rolled = false;
  bool incomplete_restore = false;
  bool normalization_insensitive = false;
  std::uint64_t volume_incompatible_features = 0;
};

struct PolicyDecision {
  MountDisposition action = MountDisposition::kReject;
  std::vector<PolicyReason> reasons;
  std::string summary;
};

PolicyDecision EvaluatePolicy(const PolicyInput& input);
std::string_view ToString(MountDisposition disposition) noexcept;
std::string_view ToString(PolicyReason reason) noexcept;

} // namespace orchard::apfs

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "orchard/apfs/volume.h"

namespace orchard::apfs {

struct ResolvedPath {
  std::string normalized_path;
  InodeRecord inode;
};

blockio::Result<ResolvedPath> LookupPath(const VolumeContext& volume, std::string_view path);
blockio::Result<std::vector<DirectoryEntryRecord>> ListDirectory(const VolumeContext& volume,
                                                                 std::uint64_t directory_inode_id);
blockio::Result<std::vector<DirectoryEntryRecord>> ListDirectory(const VolumeContext& volume,
                                                                 std::string_view path);

} // namespace orchard::apfs

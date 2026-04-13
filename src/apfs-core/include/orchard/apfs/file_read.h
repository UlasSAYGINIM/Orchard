#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "orchard/apfs/path_lookup.h"

namespace orchard::apfs {

struct FileMetadata {
  std::uint64_t object_id = 0;
  std::uint64_t logical_size = 0;
  std::uint64_t allocated_size = 0;
  std::uint64_t internal_flags = 0;
  std::uint64_t creation_time_unix_nanos = 0;
  std::uint64_t last_access_time_unix_nanos = 0;
  std::uint64_t last_write_time_unix_nanos = 0;
  std::uint64_t change_time_unix_nanos = 0;
  InodeKind kind = InodeKind::kUnknown;
  std::uint32_t child_count = 0;
  std::uint32_t link_count = 0;
  std::uint16_t mode = 0;
  bool sparse = false;
  CompressionInfo compression;
};

struct FileReadRequest {
  std::uint64_t inode_id = 0;
  std::uint64_t offset = 0;
  std::size_t size = 0;
};

blockio::Result<FileMetadata> GetFileMetadata(const VolumeContext& volume, std::uint64_t inode_id);

blockio::Result<std::vector<std::uint8_t>> ReadFileRange(const VolumeContext& volume,
                                                         const FileReadRequest& request);

blockio::Result<std::vector<std::uint8_t>> ReadWholeFile(const VolumeContext& volume,
                                                         std::uint64_t inode_id);

} // namespace orchard::apfs

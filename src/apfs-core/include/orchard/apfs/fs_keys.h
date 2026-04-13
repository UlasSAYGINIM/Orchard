#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "orchard/apfs/format.h"

namespace orchard::apfs {

enum class FsRecordType : std::uint8_t {
  kUnknown = 0U,
  kInode = 3U,
  kXattr = 4U,
  kFileExtent = 8U,
  kDirRecord = 9U,
};

struct FsKeyHeader {
  std::uint64_t object_id = 0;
  FsRecordType type = FsRecordType::kUnknown;
};

struct InodeKey {
  FsKeyHeader header;
};

struct DirectoryRecordKey {
  FsKeyHeader header;
  std::string name;
};

struct FileExtentKey {
  FsKeyHeader header;
  std::uint64_t logical_address = 0;
};

struct XattrKey {
  FsKeyHeader header;
  std::string name;
};

blockio::Result<FsKeyHeader> ParseFsKeyHeader(std::span<const std::uint8_t> bytes);
blockio::Result<InodeKey> ParseInodeKey(std::span<const std::uint8_t> bytes);
blockio::Result<DirectoryRecordKey> ParseDirectoryRecordKey(std::span<const std::uint8_t> bytes);
blockio::Result<FileExtentKey> ParseFileExtentKey(std::span<const std::uint8_t> bytes);
blockio::Result<XattrKey> ParseXattrKey(std::span<const std::uint8_t> bytes);
std::string_view ToString(FsRecordType type) noexcept;

} // namespace orchard::apfs

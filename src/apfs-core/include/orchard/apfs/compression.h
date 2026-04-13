#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "orchard/apfs/format.h"

namespace orchard::apfs {

constexpr std::string_view kCompressionXattrName = "com.apple.decmpfs";

enum class CompressionKind {
  kNone,
  kDecmpfsUncompressedAttribute,
  kUnsupported,
};

struct CompressionInfo {
  CompressionKind kind = CompressionKind::kNone;
  std::uint32_t algorithm_id = 0;
  std::uint64_t uncompressed_size = 0;
  bool supported = true;
};

blockio::Result<CompressionInfo> ParseCompressionInfo(std::span<const std::uint8_t> bytes);
blockio::Result<std::vector<std::uint8_t>>
DecodeCompressionPayload(std::span<const std::uint8_t> bytes);
std::string_view ToString(CompressionKind kind) noexcept;

} // namespace orchard::apfs

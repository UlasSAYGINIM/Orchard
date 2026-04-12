#pragma once

#include <cstdint>
#include <span>

namespace orchard::apfs {

constexpr std::size_t kApfsObjectMagicOffset = 0x20U;

bool ProbeContainerMagic(std::span<const std::uint8_t> bytes,
                         std::size_t magic_offset = kApfsObjectMagicOffset) noexcept;
bool ProbeVolumeMagic(std::span<const std::uint8_t> bytes,
                      std::size_t magic_offset = kApfsObjectMagicOffset) noexcept;

} // namespace orchard::apfs

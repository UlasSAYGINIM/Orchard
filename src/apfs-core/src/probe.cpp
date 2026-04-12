#include "orchard/apfs/probe.h"

#include <array>

namespace orchard::apfs {
namespace {

constexpr std::array<std::uint8_t, 4> kNxsbMagic{
    static_cast<std::uint8_t>('N'),
    static_cast<std::uint8_t>('X'),
    static_cast<std::uint8_t>('S'),
    static_cast<std::uint8_t>('B'),
};

constexpr std::array<std::uint8_t, 4> kApsbMagic{
    static_cast<std::uint8_t>('A'),
    static_cast<std::uint8_t>('P'),
    static_cast<std::uint8_t>('S'),
    static_cast<std::uint8_t>('B'),
};

bool ProbeMagic(std::span<const std::uint8_t> bytes, const std::size_t magic_offset,
                const std::span<const std::uint8_t> magic) noexcept {
  if (bytes.size() < magic_offset + magic.size()) {
    return false;
  }

  for (std::size_t index = 0; index < magic.size(); ++index) {
    if (bytes[magic_offset + index] != magic[index]) {
      return false;
    }
  }

  return true;
}

} // namespace

bool ProbeContainerMagic(std::span<const std::uint8_t> bytes,
                         const std::size_t magic_offset) noexcept {
  return ProbeMagic(bytes, magic_offset, kNxsbMagic);
}

bool ProbeVolumeMagic(std::span<const std::uint8_t> bytes,
                      const std::size_t magic_offset) noexcept {
  return ProbeMagic(bytes, magic_offset, kApsbMagic);
}

} // namespace orchard::apfs

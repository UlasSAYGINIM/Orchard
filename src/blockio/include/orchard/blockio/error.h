#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace orchard::blockio {

enum class ErrorCode {
  kInvalidArgument,
  kNotFound,
  kUnsupportedTarget,
  kAccessDenied,
  kOpenFailed,
  kReadFailed,
  kShortRead,
  kOutOfRange,
  kIoctlFailed,
  kInvalidFormat,
  kCorruptData,
  kNotImplemented,
};

struct Error {
  ErrorCode code = ErrorCode::kInvalidArgument;
  std::string message;
  std::uint32_t system_code = 0;
};

std::string_view ToString(ErrorCode code) noexcept;

} // namespace orchard::blockio

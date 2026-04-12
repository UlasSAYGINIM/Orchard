#include "orchard/blockio/error.h"

namespace orchard::blockio {

std::string_view ToString(ErrorCode code) noexcept {
  switch (code) {
  case ErrorCode::kInvalidArgument:
    return "invalid_argument";
  case ErrorCode::kNotFound:
    return "not_found";
  case ErrorCode::kUnsupportedTarget:
    return "unsupported_target";
  case ErrorCode::kAccessDenied:
    return "access_denied";
  case ErrorCode::kOpenFailed:
    return "open_failed";
  case ErrorCode::kReadFailed:
    return "read_failed";
  case ErrorCode::kShortRead:
    return "short_read";
  case ErrorCode::kOutOfRange:
    return "out_of_range";
  case ErrorCode::kIoctlFailed:
    return "ioctl_failed";
  case ErrorCode::kInvalidFormat:
    return "invalid_format";
  case ErrorCode::kCorruptData:
    return "corrupt_data";
  case ErrorCode::kNotImplemented:
    return "not_implemented";
  }

  return "invalid_argument";
}

} // namespace orchard::blockio

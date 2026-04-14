#include "orchard/mount_service/types.h"

#include <utility>

namespace orchard::mount_service {

blockio::Error MakeMountServiceError(const blockio::ErrorCode code, std::string message,
                                     const std::uint32_t system_code) {
  return blockio::Error{
      .code = code,
      .message = std::move(message),
      .system_code = system_code,
  };
}

} // namespace orchard::mount_service

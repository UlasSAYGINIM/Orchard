#pragma once

#include <memory>
#include <string>
#include <vector>

#include "orchard/blockio/result.h"

namespace orchard::mount_service {

struct DeviceInterfaceInfo {
  std::wstring device_path;
};

class DeviceEnumerator {
public:
  virtual ~DeviceEnumerator() = default;

  [[nodiscard]] virtual blockio::Result<std::vector<DeviceInterfaceInfo>>
  EnumeratePresentDiskInterfaces() = 0;
};

std::unique_ptr<DeviceEnumerator> CreateDefaultDeviceEnumerator();

} // namespace orchard::mount_service

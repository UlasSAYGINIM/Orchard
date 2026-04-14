#pragma once

#include <cstdint>
#include <string_view>

#include "orchard/blockio/result.h"

namespace orchard::mount_service {

enum class ServiceState {
  kCreated,
  kStartPending,
  kRunning,
  kStopPending,
  kStopped,
};

struct ServiceStateSnapshot {
  ServiceState state = ServiceState::kCreated;
  std::uint32_t checkpoint = 0;
  std::uint32_t wait_hint_ms = 0;
  bool accepts_stop = false;
};

std::string_view ToString(ServiceState state) noexcept;

class ServiceStateMachine {
public:
  [[nodiscard]] const ServiceStateSnapshot& snapshot() const noexcept {
    return snapshot_;
  }

  [[nodiscard]] blockio::Result<ServiceStateSnapshot>
  TransitionTo(ServiceState next_state, std::uint32_t wait_hint_ms = 0);

private:
  ServiceStateSnapshot snapshot_;
};

} // namespace orchard::mount_service

#include "orchard/mount_service/service_state.h"

#include <utility>

#include "orchard/mount_service/types.h"

namespace orchard::mount_service {
namespace {

bool IsPendingState(const ServiceState state) noexcept {
  return state == ServiceState::kStartPending || state == ServiceState::kStopPending;
}

bool AcceptsStopControl(const ServiceState state) noexcept {
  return state == ServiceState::kRunning;
}

bool IsValidTransition(const ServiceState from, const ServiceState to) noexcept {
  if (from == to) {
    return true;
  }

  switch (from) {
  case ServiceState::kCreated:
    return to == ServiceState::kStartPending || to == ServiceState::kStopped;
  case ServiceState::kStartPending:
    return to == ServiceState::kRunning || to == ServiceState::kStopPending ||
           to == ServiceState::kStopped;
  case ServiceState::kRunning:
    return to == ServiceState::kStopPending;
  case ServiceState::kStopPending:
    return to == ServiceState::kStopped;
  case ServiceState::kStopped:
    return false;
  }

  return false;
}

} // namespace

std::string_view ToString(const ServiceState state) noexcept {
  switch (state) {
  case ServiceState::kCreated:
    return "created";
  case ServiceState::kStartPending:
    return "start_pending";
  case ServiceState::kRunning:
    return "running";
  case ServiceState::kStopPending:
    return "stop_pending";
  case ServiceState::kStopped:
    return "stopped";
  }

  return "unknown";
}

blockio::Result<ServiceStateSnapshot>
ServiceStateMachine::TransitionTo(const ServiceState next_state, const std::uint32_t wait_hint_ms) {
  if (!IsValidTransition(snapshot_.state, next_state)) {
    return MakeMountServiceError(blockio::ErrorCode::kInvalidArgument,
                                 "Invalid Orchard service-state transition from '" +
                                     std::string(ToString(snapshot_.state)) + "' to '" +
                                     std::string(ToString(next_state)) + "'.");
  }

  const auto previous_state = snapshot_.state;
  snapshot_.state = next_state;
  snapshot_.wait_hint_ms = wait_hint_ms;
  snapshot_.accepts_stop = AcceptsStopControl(next_state);

  if (IsPendingState(next_state)) {
    snapshot_.checkpoint = IsPendingState(previous_state) ? (snapshot_.checkpoint + 1U) : 1U;
  } else {
    snapshot_.checkpoint = 0U;
  }

  return snapshot_;
}

} // namespace orchard::mount_service

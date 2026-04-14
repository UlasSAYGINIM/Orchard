#include "orchard/mount_service/rescan_coordinator.h"

#include <utility>

namespace orchard::mount_service {

RescanCoordinator::RescanCoordinator(RescanTaskPoster post_task, RescanAction action)
    : post_task_(std::move(post_task)), action_(std::move(action)) {}

void RescanCoordinator::RequestRescan() {
  bool should_schedule = false;

  {
    std::scoped_lock lock(mutex_);
    if (shutdown_) {
      return;
    }

    if (running_) {
      rescan_requested_ = true;
      return;
    }

    running_ = true;
    should_schedule = true;
  }

  if (!should_schedule) {
    return;
  }

  if (!post_task_([this]() { RunScheduledRescans(); })) {
    std::scoped_lock lock(mutex_);
    running_ = false;
  }
}

void RescanCoordinator::Shutdown() noexcept {
  std::scoped_lock lock(mutex_);
  shutdown_ = true;
  rescan_requested_ = false;
}

void RescanCoordinator::RunScheduledRescans() {
  for (;;) {
    action_();

    std::scoped_lock lock(mutex_);
    if (shutdown_) {
      running_ = false;
      rescan_requested_ = false;
      return;
    }
    if (!rescan_requested_) {
      running_ = false;
      return;
    }

    rescan_requested_ = false;
  }
}

} // namespace orchard::mount_service

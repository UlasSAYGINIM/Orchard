#pragma once

#include <functional>
#include <mutex>

namespace orchard::mount_service {

using RescanTaskPoster = std::function<bool(std::function<void()>)>;
using RescanAction = std::function<void()>;

class RescanCoordinator {
public:
  RescanCoordinator(RescanTaskPoster post_task, RescanAction action);

  void RequestRescan();
  void Shutdown() noexcept;

private:
  void RunScheduledRescans();

  RescanTaskPoster post_task_;
  RescanAction action_;

  std::mutex mutex_;
  bool running_ = false;
  bool rescan_requested_ = false;
  bool shutdown_ = false;
};

} // namespace orchard::mount_service

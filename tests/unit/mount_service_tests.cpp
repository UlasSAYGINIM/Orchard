#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "orchard/mount_service/mount_registry.h"
#include "orchard/mount_service/runtime.h"
#include "orchard/mount_service/service_host.h"
#include "orchard/mount_service/service_state.h"
#include "orchard_test/test.h"

namespace {

struct FakeFactoryState {
  std::vector<orchard::fs_winfsp::MountConfig> seen_configs;
  std::vector<std::shared_ptr<int>> stop_counters;
};

class FakeManagedMountSession final : public orchard::mount_service::ManagedMountSession {
public:
  FakeManagedMountSession(std::wstring mount_point, std::wstring volume_label,
                          const std::uint64_t volume_object_id, std::string volume_name,
                          std::shared_ptr<int> stop_counter)
      : mount_point_(std::move(mount_point)), volume_label_(std::move(volume_label)),
        volume_object_id_(volume_object_id), volume_name_(std::move(volume_name)),
        stop_counter_(std::move(stop_counter)) {}

  [[nodiscard]] std::wstring_view mount_point() const noexcept override {
    return mount_point_;
  }
  [[nodiscard]] std::wstring_view volume_label() const noexcept override {
    return volume_label_;
  }
  [[nodiscard]] std::uint64_t volume_object_id() const noexcept override {
    return volume_object_id_;
  }
  [[nodiscard]] std::string_view volume_name() const noexcept override {
    return volume_name_;
  }

  void Stop() noexcept override {
    ++(*stop_counter_);
  }

private:
  std::wstring mount_point_;
  std::wstring volume_label_;
  std::uint64_t volume_object_id_ = 0;
  std::string volume_name_;
  std::shared_ptr<int> stop_counter_;
};

class FakeMountSessionFactory final : public orchard::mount_service::MountSessionFactory {
public:
  explicit FakeMountSessionFactory(std::shared_ptr<FakeFactoryState> state) : state_(std::move(state)) {}

  [[nodiscard]] orchard::blockio::Result<orchard::mount_service::ManagedMountSessionHandle>
  Start(const orchard::fs_winfsp::MountConfig& config) override {
    state_->seen_configs.push_back(config);
    auto stop_counter = std::make_shared<int>(0);
    state_->stop_counters.push_back(stop_counter);

    orchard::mount_service::ManagedMountSessionHandle session(new FakeManagedMountSession(
        config.mount_point, L"Fake Volume", 42U, "Fake Volume", std::move(stop_counter)));
    return session;
  }

private:
  std::shared_ptr<FakeFactoryState> state_;
};

orchard::mount_service::MountRequest MakeMountRequest(std::wstring mount_id, std::wstring mount_point) {
  orchard::mount_service::MountRequest request;
  request.mount_id = std::move(mount_id);
  request.config.target_path = "fixture.img";
  request.config.mount_point = std::move(mount_point);
  return request;
}

void ServiceStateMachineAcceptsExpectedTransitions() {
  orchard::mount_service::ServiceStateMachine state_machine;

  auto start_pending = state_machine.TransitionTo(orchard::mount_service::ServiceState::kStartPending, 2000U);
  ORCHARD_TEST_REQUIRE(start_pending.ok());
  ORCHARD_TEST_REQUIRE(start_pending.value().state ==
                       orchard::mount_service::ServiceState::kStartPending);
  ORCHARD_TEST_REQUIRE(start_pending.value().checkpoint == 1U);

  auto running = state_machine.TransitionTo(orchard::mount_service::ServiceState::kRunning);
  ORCHARD_TEST_REQUIRE(running.ok());
  ORCHARD_TEST_REQUIRE(running.value().accepts_stop);

  auto invalid = state_machine.TransitionTo(orchard::mount_service::ServiceState::kStartPending);
  ORCHARD_TEST_REQUIRE(!invalid.ok());

  auto stop_pending = state_machine.TransitionTo(orchard::mount_service::ServiceState::kStopPending, 1500U);
  ORCHARD_TEST_REQUIRE(stop_pending.ok());
  auto stopped = state_machine.TransitionTo(orchard::mount_service::ServiceState::kStopped);
  ORCHARD_TEST_REQUIRE(stopped.ok());
}

void MountRegistryRejectsDuplicateMountIdsAndMountPoints() {
  auto state = std::make_shared<FakeFactoryState>();
  orchard::mount_service::MountRegistry registry(
      std::make_unique<FakeMountSessionFactory>(state));

  auto first_mount = registry.MountVolume(MakeMountRequest(L"alpha", L"R:"));
  ORCHARD_TEST_REQUIRE(first_mount.ok());

  auto duplicate_id = registry.MountVolume(MakeMountRequest(L"alpha", L"S:"));
  ORCHARD_TEST_REQUIRE(!duplicate_id.ok());

  auto duplicate_mount_point = registry.MountVolume(MakeMountRequest(L"beta", L"r:"));
  ORCHARD_TEST_REQUIRE(!duplicate_mount_point.ok());

  auto mounts = registry.ListMounts();
  ORCHARD_TEST_REQUIRE(mounts.size() == 1U);

  auto unmount_result = registry.UnmountVolume(orchard::mount_service::UnmountRequest{.mount_id = L"alpha"});
  ORCHARD_TEST_REQUIRE(unmount_result.ok());
  ORCHARD_TEST_REQUIRE(state->stop_counters.size() == 1U);
  ORCHARD_TEST_REQUIRE(*state->stop_counters.front() == 1);
}

void ServiceRuntimeStopIsIdempotentAndStopsMountedSessions() {
  auto state = std::make_shared<FakeFactoryState>();
  std::vector<orchard::mount_service::ServiceState> seen_states;

  orchard::mount_service::ServiceRuntime runtime(
      {}, std::make_unique<FakeMountSessionFactory>(state),
      [&seen_states](const orchard::mount_service::ServiceStateSnapshot& snapshot) {
        seen_states.push_back(snapshot.state);
      });

  auto start_result = runtime.Start();
  ORCHARD_TEST_REQUIRE(start_result.ok());

  auto mount_result = runtime.MountVolume(MakeMountRequest(L"runtime-alpha", L"R:"));
  ORCHARD_TEST_REQUIRE(mount_result.ok());

  auto list_result = runtime.ListMounts();
  ORCHARD_TEST_REQUIRE(list_result.ok());
  ORCHARD_TEST_REQUIRE(list_result.value().size() == 1U);

  runtime.Stop();
  runtime.Stop();

  ORCHARD_TEST_REQUIRE(!seen_states.empty());
  ORCHARD_TEST_REQUIRE(seen_states.front() ==
                       orchard::mount_service::ServiceState::kStartPending);
  ORCHARD_TEST_REQUIRE(seen_states.back() ==
                       orchard::mount_service::ServiceState::kStopped);
  ORCHARD_TEST_REQUIRE(state->stop_counters.size() == 1U);
  ORCHARD_TEST_REQUIRE(*state->stop_counters.front() == 1);
}

void ServiceHostCommandLineParsesConsoleMountOptions() {
  std::vector<char*> argv{
      const_cast<char*>("orchard-service-host"),
      const_cast<char*>("--console"),
      const_cast<char*>("--service-name"),
      const_cast<char*>("OrchardTestSvc"),
      const_cast<char*>("--target"),
      const_cast<char*>("fixture.img"),
      const_cast<char*>("--mountpoint"),
      const_cast<char*>("R:"),
      const_cast<char*>("--volume-name"),
      const_cast<char*>("Data"),
      const_cast<char*>("--hold-ms"),
      const_cast<char*>("5000"),
  };

  auto parse_result = orchard::mount_service::ParseServiceHostCommandLine(
      static_cast<int>(argv.size()), argv.data());
  ORCHARD_TEST_REQUIRE(parse_result.ok());
  ORCHARD_TEST_REQUIRE(parse_result.value().mode ==
                       orchard::mount_service::ServiceLaunchMode::kConsole);
  ORCHARD_TEST_REQUIRE(parse_result.value().service.service_name == L"OrchardTestSvc");
  const auto& startup_mount_optional = parse_result.value().startup_mount;
  if (!startup_mount_optional.has_value()) {
    throw orchard_test::Failure("startup_mount was not populated.");
  }
  const auto& startup_mount = startup_mount_optional.value();
  ORCHARD_TEST_REQUIRE(startup_mount.config.mount_point == L"R:");
  const auto& selector_name_optional = startup_mount.config.selector.name;
  if (!selector_name_optional.has_value()) {
    throw orchard_test::Failure("selector.name was not populated.");
  }
  const auto& selector_name = selector_name_optional.value();
  ORCHARD_TEST_REQUIRE(selector_name == "Data");
  const auto& hold_timeout_optional = parse_result.value().hold_timeout_ms;
  if (!hold_timeout_optional.has_value()) {
    throw orchard_test::Failure("hold_timeout_ms was not populated.");
  }
  const auto hold_timeout_ms = hold_timeout_optional.value();
  ORCHARD_TEST_REQUIRE(hold_timeout_ms == 5000U);
}

} // namespace

int main() {
  return orchard_test::RunTests({
      {"ServiceStateMachineAcceptsExpectedTransitions",
       &ServiceStateMachineAcceptsExpectedTransitions},
      {"MountRegistryRejectsDuplicateMountIdsAndMountPoints",
       &MountRegistryRejectsDuplicateMountIdsAndMountPoints},
      {"ServiceRuntimeStopIsIdempotentAndStopsMountedSessions",
       &ServiceRuntimeStopIsIdempotentAndStopsMountedSessions},
      {"ServiceHostCommandLineParsesConsoleMountOptions",
       &ServiceHostCommandLineParsesConsoleMountOptions},
  });
}

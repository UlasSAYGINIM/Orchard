#include <iostream>

#include "orchard/mount_service/service_host.h"

int main(int argc, char** argv) {
  auto options_result = orchard::mount_service::ParseServiceHostCommandLine(argc, argv);
  if (!options_result.ok()) {
    std::cerr << options_result.error().message << '\n';
    return 1;
  }

  return orchard::mount_service::RunServiceHost(options_result.value());
}

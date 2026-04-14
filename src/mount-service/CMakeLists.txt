add_library(
  orchard_mount_service
  STATIC
    include/orchard/mount_service/mount_registry.h
    include/orchard/mount_service/device_discovery.h
    include/orchard/mount_service/device_enumerator.h
    include/orchard/mount_service/device_inventory.h
    include/orchard/mount_service/device_monitor.h
    include/orchard/mount_service/rescan_coordinator.h
    include/orchard/mount_service/runtime.h
    include/orchard/mount_service/service_host.h
    include/orchard/mount_service/service_state.h
    include/orchard/mount_service/types.h
    src/device_discovery.cpp
    src/device_enumerator.cpp
    src/device_inventory.cpp
    src/device_monitor.cpp
    src/mount_registry.cpp
    src/rescan_coordinator.cpp
    src/runtime.cpp
    src/service_host.cpp
    src/service_state.cpp
    src/types.cpp
)

target_include_directories(
  orchard_mount_service
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(
  orchard_mount_service
  PUBLIC
    orchard_fs_winfsp
  PRIVATE
    advapi32
    cfgmgr32
)

orchard_configure_target(orchard_mount_service)

add_executable(
  orchard_service_host
  src/main.cpp
)

target_link_libraries(
  orchard_service_host
  PRIVATE
    orchard_mount_service
)

set_target_properties(
  orchard_service_host
  PROPERTIES
    OUTPUT_NAME "orchard-service-host"
)

orchard_configure_target(orchard_service_host)

if(TARGET WinFsp::WinFsp)
  orchard_copy_runtime_dlls(orchard_service_host)
  add_custom_command(
    TARGET orchard_service_host
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:WinFsp::WinFsp>"
      "$<TARGET_FILE_DIR:orchard_service_host>"
  )
endif()

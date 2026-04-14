orchard_add_cpp_test(
  orchard_unit_blockio
  SOURCES
    inspection_target_tests.cpp
  LIBRARIES
    orchard_blockio
)

orchard_add_cpp_test(
  orchard_unit_blockio_reader
  SOURCES
    reader_tests.cpp
  LIBRARIES
    orchard_blockio
)

orchard_add_cpp_test(
  orchard_unit_apfs
  SOURCES
    apfs_tests.cpp
  LIBRARIES
    orchard_apfs_core
    orchard_fs_winfsp
)

orchard_add_cpp_test(
  orchard_unit_mount_service
  SOURCES
    mount_service_tests.cpp
  LIBRARIES
    orchard_mount_service
)

target_compile_definitions(
  orchard_unit_apfs
  PRIVATE
    ORCHARD_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

if(TARGET WinFsp::WinFsp)
  orchard_copy_runtime_dlls(orchard_unit_apfs)
  orchard_copy_runtime_dlls(orchard_unit_mount_service)
endif()

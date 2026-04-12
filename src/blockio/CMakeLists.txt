add_library(
  orchard_blockio
  STATIC
    src/error.cpp
    src/inspection_target.cpp
    src/reader.cpp
)

target_include_directories(
  orchard_blockio
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

orchard_configure_target(orchard_blockio)

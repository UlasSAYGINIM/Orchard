set(ORCHARD_SAMPLE_MANIFEST "${PROJECT_SOURCE_DIR}/tests/corpus/manifests/sample-fixtures.json")
set(ORCHARD_DIRECT_SAMPLE_IMAGE "${PROJECT_SOURCE_DIR}/tests/corpus/samples/plain-user-data.img")
set(ORCHARD_GPT_SAMPLE_IMAGE "${PROJECT_SOURCE_DIR}/tests/corpus/samples/gpt-user-data.img")

add_test(
  NAME orchard_validate_fixture_manifest
  COMMAND
    "${CMAKE_COMMAND}"
      -DMANIFEST_PATH=${ORCHARD_SAMPLE_MANIFEST}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/validate_fixture_manifest.cmake"
)

add_test(
  NAME orchard_inspect_direct_output
  COMMAND
    "${CMAKE_COMMAND}"
      -DEXECUTABLE_PATH=$<TARGET_FILE:orchard_inspect>
      -DTARGET_PATH=${ORCHARD_DIRECT_SAMPLE_IMAGE}
      -DEXPECTED_LAYOUT=direct_container
      "-DEXPECTED_VOLUME_NAME=Orchard Data"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/validate_orchard_inspect_output.cmake"
)

add_test(
  NAME orchard_inspect_gpt_output
  COMMAND
    "${CMAKE_COMMAND}"
      -DEXECUTABLE_PATH=$<TARGET_FILE:orchard_inspect>
      -DTARGET_PATH=${ORCHARD_GPT_SAMPLE_IMAGE}
      -DEXPECTED_LAYOUT=guid_partition_table
      "-DEXPECTED_VOLUME_NAME=GPT Data"
      "-DEXPECTED_PARTITION_NAME=Orchard GPT"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/validate_orchard_inspect_output.cmake"
)

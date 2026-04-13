if(NOT DEFINED EXECUTABLE_PATH)
  message(FATAL_ERROR "EXECUTABLE_PATH is required.")
endif()

if(NOT DEFINED TARGET_PATH)
  message(FATAL_ERROR "TARGET_PATH is required.")
endif()

execute_process(
  COMMAND "${EXECUTABLE_PATH}" --target "${TARGET_PATH}"
  RESULT_VARIABLE inspect_result
  OUTPUT_VARIABLE inspect_output
  ERROR_VARIABLE inspect_error
)

if(NOT inspect_result EQUAL 0)
  message(FATAL_ERROR "orchard-inspect failed: ${inspect_error}")
endif()

set(required_fragments
  "\"inspection_status\": \"success\""
  "\"name\": \"Snapshot Data\""
  "\"action\": \"MountReadOnly\""
  "\"SnapshotsPresent\""
)

foreach(required_fragment IN LISTS required_fragments)
  string(FIND "${inspect_output}" "${required_fragment}" match_position)
  if(match_position EQUAL -1)
    message(FATAL_ERROR "Missing expected fragment '${required_fragment}' in output:\n${inspect_output}")
  endif()
endforeach()

message(STATUS "Snapshot policy output validation passed for ${TARGET_PATH}.")

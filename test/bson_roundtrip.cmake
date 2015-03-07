# bson_roundtrip:
#
# Given a JSON or BSON file, perform a roundtrip conversion on it and compare to the original.
# The test will fail if the input and output are not identical.
#
# The following variables must be set:
#   JSON2BSON - the path to the json2bson executable.
#   BSON2JSON - the path to the bson2json executable.
#   TESTNAME  - a name for the test that can serve as part of a temporary
#               filename unique to this test (so tests can run in parallel).
#   INPUT     - path to an input JSON or BSON file to convert and compare.
#
# The INPUT filename

if (NOT JSON2BSON)
  message(FATAL_ERROR "JSON2BSON must be defined")
endif()
if (NOT BSON2JSON)
  message(FATAL_ERROR "BSON2JSON must be defined")
endif()
if (NOT TESTNAME)
  message(FATAL_ERROR "TESTNAME must be defined")
endif()

if ("${INPUT}" MATCHES "json$")

  # Convert the JSON to BSON
  execute_process(
    COMMAND ${JSON2BSON} ${INPUT} ${TESTNAME}.test.bson
    OUTPUT_FILE ${TESTNAME}.json2bson.log
    ERROR_VARIABLE TEST_ERROR
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: ${JSON2BSON} failed with status ${TEST_RESULT}.\n${TEST_ERROR}")
  endif()

  # Convert the BSON back to JSON
  execute_process(
    COMMAND ${BSON2JSON} ${TESTNAME}.test.bson ${TESTNAME}.test.json
    OUTPUT_FILE ${TESTNAME}.bson2json.log
    ERROR_VARIABLE TEST_ERROR
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: ${BSON2JSON} failed with status ${TEST_RESULT}.\n${TEST_ERROR}")
  endif()

  # Compare the generated JSON to the input JSON.
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${INPUT} ${TESTNAME}.test.json
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: Round trip does not match test input.")
  endif( TEST_RESULT )

elseif ("${INPUT}" MATCHES "bson$")

  # Convert the BSON to JSON
  execute_process(
    COMMAND ${BSON2JSON} ${INPUT} ${TESTNAME}.test.json
    OUTPUT_FILE ${TESTNAME}.bson2json.log
    ERROR_VARIABLE TEST_ERROR
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: ${BSON2JSON} failed with status ${TEST_RESULT}.\n${TEST_ERROR}")
  endif()

  # Convert the JSON back to BSON
  execute_process(
    COMMAND ${JSON2BSON} ${TESTNAME}.test.json ${TESTNAME}.test.bson
    OUTPUT_FILE ${TESTNAME}.json2bson.log
    ERROR_VARIABLE TEST_ERROR
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: ${JSON2BSON} failed with status ${TEST_RESULT}.\n${TEST_ERROR}")
  endif()

  # Compare the generated JSON to the input JSON.
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${INPUT} ${TESTNAME}.test.bson
    RESULT_VARIABLE TEST_RESULT
  )
  if (TEST_RESULT)
    message(FATAL_ERROR "Failed ${TESTNAME}: Round trip does not match test input.")
  endif( TEST_RESULT )

else()

  message(FATAL_ERROR "INPUT file \"${INPUT}\" does not end with json or bson.")

endif()

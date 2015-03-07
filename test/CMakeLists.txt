function(bson_roundtrip_test NAME FILE)
  add_test(
    NAME ${NAME}
    COMMAND ${CMAKE_COMMAND}
      -DJSON2BSON=$<TARGET_FILE:json2bson>
      -DBSON2JSON=$<TARGET_FILE:bson2json>
      -DTESTNAME=${NAME}
      -DINPUT=${FILE}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/bson_roundtrip.cmake"
  )
endfunction()

bson_roundtrip_test(test_discern_json ${CMAKE_CURRENT_SOURCE_DIR}/json/test_discern.json)
bson_roundtrip_test(test_discern2_json ${CMAKE_CURRENT_SOURCE_DIR}/json/test_discern2.json)
bson_roundtrip_test(test_discern2_bson ${CMAKE_CURRENT_SOURCE_DIR}/bson/test_discern2.bson)

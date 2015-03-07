project(cJSON_BSON)
cmake_minimum_required(VERSION 3.1)

add_library(cJSON
  cJSON.c
  cJSON_BSON.c
)

add_executable(json2bson json2bson.cxx)
target_link_libraries(json2bson cJSON)

add_executable(bson2json bson2json.cxx)
target_link_libraries(bson2json cJSON)

install(
  TARGETS cJSON json2bson bson2json
  EXPORT cJSON
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
export(PACKAGE cJSON)
export(TARGETS cJSON APPEND FILE cJSON-exports.cmake)
install(
  FILES
    cJSON.h
    cJSON_BSON.h
  DESTINATION include
)
install(
  EXPORT cJSON
  DESTINATION lib/cmake/cJSON
)

enable_testing()
include(CTest)
add_subdirectory(test)

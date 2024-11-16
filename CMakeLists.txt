cmake_minimum_required(VERSION 3.13.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(axrgb)

# #####################################################
# # Git stuff
# #####################################################
execute_process(
  COMMAND git log --pretty=format:%h -n 1
  OUTPUT_VARIABLE GIT_REV
  ERROR_QUIET
)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_BRANCH "N/A")
    set(GIT_REV "N/A")
    set(GIT_DIRTY "")
else()
  execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND git describe --exact-match --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND git diff-index --quiet HEAD --
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE VERSION_COMMIT_DIRTY
  )
  if(VERSION_COMMIT_DIRTY)
    set(GIT_DIRTY "-dirty")
  else()
    set(GIT_DIRTY "")
  endif()
endif()
idf_build_set_property(COMPILE_DEFINITIONS "-DAXRGB_FW_HASH=\"${GIT_REV}${GIT_DIRTY}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DAXRGB_FW_BRANCH=\"${GIT_BRANCH}\"" APPEND)
# #####################################################

set(BUILD_DIR ${CMAKE_BINARY_DIR})
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-parameter" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_SSID=\"$ENV{WIFI_SSID}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_PASS=\"$ENV{WIFI_PASS}\"" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_SERVER=\"$ENV{MQTT_URI}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_USER=\"$ENV{MQTT_USER}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_PASS=\"$ENV{MQTT_PASS}\"" APPEND)

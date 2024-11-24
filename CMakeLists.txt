cmake_minimum_required(VERSION 3.13.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(BUILD_DIR ${CMAKE_BINARY_DIR})

project(aeolian)

set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# #####################################################
# # components, lots of components.
# #####################################################
set(COMPONENTS
      app
      app_trace
      app_update
      app_update 
      bootloader 
      bootloader_init 
      bootloader_support 
      console
      cxx
      driver 
      efuse 
      espfs 
      esptool_py 
      esp_app_format 
      esp_bootloader_format 
      esp_common
      esp_driver_gpio
      esp_driver_gptimer
      esp_driver_rmt 
      esp_driver_usb_serial_jtag
      esp_event
      esp_gdbstub 
      esp_http_server 
      esp_hw_support 
      esp_netif 
      esp_netif_stack 
      esp_partition
      esp_phy
      esp_pm
      esp_psram 
      esp_ringbuf
      esp_rom
      esp_system
      esp_timer
      esp_wifi
      espcoredump
      fatfs
      freertos 
      hal 
      heap
      http_parser
      idf_test
      json
      jsmn
      log
      lwip
      mqtt 
      main 
      nvs_flash 
      openthread
      partition_table
      pthread
      soc 
      spi_flash
      spiffs
      tcp_transport
      tcpip_adapter 
      usb
      vfs 
      wear_levelling
      wifi_provisioning
      wpa_supplicant
      xtensa
    )


# #####################################################
# # Git stuff
# #####################################################
if (EXISTS "${CMAKE_SOURCE_DIR}/.git")
  find_package(Git)
  if (Git_FOUND)
    execute_process(
      COMMAND git branch --show-current
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git config user.name
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_USER_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git rev-list --count HEAD
      OUTPUT_VARIABLE GIT_COMMIT_REVISION
      ERROR_QUIET
    )
    execute_process(
      COMMAND git rev-parse --short --verify HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_COMMIT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git describe --exact-match --tags
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_COMMIT_TAG
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
else()
  set(GIT_COMMIT_REVISION "")
  set(GIT_COMMIT_HASH "<unknown dev>")
  set(GIT_COMMIT_TAG "")
  set(GIT_DIRTY "")
endif()
#string(TIMESTAMP TODAY "%Y%m%d")
#set(VERSION_REVISION "-${TODAY}-${GIT_COMMIT_HASH}${GIT_DIRTY}")
idf_build_set_property(COMPILE_DEFINITIONS "-DAEOLIAN_FW_HASH=\"${GIT_COMMIT_HASH}${GIT_DIRTY}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DAEOLIAN_FW_BRANCH=\"${GIT_BRANCH}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DAEOLIAN_FW_GITUSER=\"${GIT_USER_NAME}\"" APPEND)
# #####################################################

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-parameter" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)

# #####################################################
if (CONFIG_PM_ENABLE)
  set(PM_ENABLED "yes")
else()
  set(PM_ENABLED "no")
endif(CONFIG_PM_ENABLE)
idf_build_set_property(COMPILE_DEFINITIONS "-DPM_ENABLED=\"${PM_ENABLED}\"" APPEND)

# #####################################################
idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_SSID=\"$ENV{WIFI_SSID}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_PASS=\"$ENV{WIFI_PASS}\"" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_SERVER=\"$ENV{MQTT_URI}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_USER=\"$ENV{MQTT_USER}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_PASS=\"$ENV{MQTT_PASS}\"" APPEND)

# #####################################################
# Fix jsmn not compiling correctly
idf_build_set_property(COMPILE_DEFINITIONS "-DJSMN_STATIC" APPEND)

idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color" APPEND)
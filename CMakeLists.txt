cmake_minimum_required(VERSION 3.13.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(axrgb)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-parameter" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-sign-compare" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_SSID=\"$ENV{WIFI_SSID}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DWIFI_PASS=\"$ENV{WIFI_PASS}\"" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_SERVER=\"$ENV{MQTT_URI}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_USER=\"$ENV{MQTT_USER}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DMQTT_PASS=\"$ENV{MQTT_PASS}\"" APPEND)

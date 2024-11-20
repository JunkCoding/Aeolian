idf_component_register(SRCS
  "app_cctv.cpp"
  "app_configs.cpp"
  "app_flash.cpp"
  "app_gpio.cpp"
  "app_httpd.cpp"
  "app_lightcontrol.cpp"
  "app_main.cpp"
  "app_mqtt.cpp"
  "app_mqtt_config.cpp"
  "app_mqtt_settings.cpp"
  "app_ota.cpp"
  "app_schedule.cpp"
  "app_sntp.cpp"
  "app_task.cpp"
  "app_timer.cpp"
  "app_utils.cpp"
  "app_websockets.cpp"
  "app_wifi.cpp"
  "app_yuarel.cpp"
  "cgi_config.cpp"
  "cgi_lightcontrol.cpp"
  "colours.cpp"
  "device_control.cpp"
  "lib8.cpp"
  "moonphase.cpp"
  "newlib8.cpp"
  "patterns.cpp"
  "sundial.cpp"
  "ws2812_driver.cpp"
  "FreeRTOS-openocd.cpp"
  INCLUDE_DIRS "." "../include/"
  REQUIRED_IDF_TARGETS "esp32"
  )

idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-function" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-variable" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-but-set-variable" APPEND)

#target_compile_options(${COMPONENT_LIB} PRIVATE ${compile_args})
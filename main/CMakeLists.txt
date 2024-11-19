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
  REQUIRES esp_http_server espfs vfs
  PRIV_REQUIRES partition_table esp_app_format esp_bootloader_format esp_http_server esp_hw_support esp_netif esp_netif_stack esp_partition esp_psram mqtt
  PRIV_REQUIRES app_update bootloader bootloader_support driver efuse esp_app_format esptool_py esp_wifi esp_https_ota nvs_flash soc spi_flash
  PRIV_REQUIRES esp_common esp_hw_support esp_driver_gpio esp_driver_rmt esp_system jsmn
  )

idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-function" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-variable" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-unused-but-set-variable" APPEND)

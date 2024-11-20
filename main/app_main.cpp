


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include <soc/rmt_struct.h>

#include <driver/periph_ctrl.h>
#include <driver/timer.h>
#include <driver/i2s.h>
#include <driver/uart.h>

#include <esp_task_wdt.h>
#include <esp_heap_task_info.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_pm.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include "app_wifi.h"
#include "app_httpd.h"
#include "app_main.h"
#include "app_utils.h"
#include "app_lightcontrol.h"
#include "app_sntp.h"
#include "sundial.h"
#include "device_control.h"
#include "app_schedule.h"
#include "app_flash.h"
#include "app_cctv.h"
#include "app_gpio.h"
#include "app_mqtt_config.h"
#include "app_mqtt.h"
#include "app_httpd.h"
#include "moonphase.h"
#include "app_timer.h"

lightcheck_t lightcheck = {0, 0, 0, 0, 0, 0, false};

// ************************************************************
// *                    A P P   M A I N                       *
// ************************************************************
extern "C" void app_main(void)
{
  //esp_err_t err;
  // -----------------------------------------------------------
  // Set our console to our requested monitor
  // rate (CONFIG_ESPTOOLPY_MONITOR_BAUD)
  // -----------------------------------------------------------
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  static const uart_config_t uart_cfg = {
    .baud_rate            = CONFIG_ESPTOOLPY_MONITOR_BAUD,
    .data_bits            = UART_DATA_8_BITS,
    .parity               = UART_PARITY_DISABLE,
    .stop_bits            = UART_STOP_BITS_1,
    .flow_ctrl            = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh  = 127,
  };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour
  uart_param_config(UART_NUM_0, &uart_cfg);

  // -----------------------------------------------------------
  // Ensure lights stay off during initialisation
  // -----------------------------------------------------------
  lightsPause(PAUSE_BOOT);

  // -----------------------------------------------------------
  // Seed our pseudo random number generator.
  // -----------------------------------------------------------
  prng_seed();

#ifdef CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE
  // -----------------------------------------------------------
  // If this option is set, the app must explicitly reset, feed,
  // or disable the rtc_wdt in it's own code.
  // -----------------------------------------------------------
  rtc_wdt_protect_off();
  rtc_wdt_disable();
#endif

#if CONFIG_USE_TASK_WDT
  F_LOGI(true, true, LC_GREY, "Initialising TWDT");
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms     = TWDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
    .trigger_panic  = true,
  };
  ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
#endif /* CONFIG_USE_TASK_WDT */

#ifndef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
  // F_LOGW(true, true, LC_BRIGHT_YELLOW, "esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0))");
  // FIXME: esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0));
#endif /* CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0 */

#ifndef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU1
  // F_LOGW(true, true, LC_BRIGHT_YELLOW, "esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1))");
  // FIXME: esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1));
#endif /* CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU1 */

  // -----------------------------------------------------------
  // Warn GDBSTUB_ENABLED is set
  // -----------------------------------------------------------
#if defined (CONFIG_ESP_GDBSTUB_ENABLED)
  F_LOGW(true, true, LC_BRIGHT_RED, "gdbstub enabled");
#endif

  // -----------------------------------------------------------
  // Init NVS, SPI and Update the bootcount
  // -----------------------------------------------------------
  boot_init();

  // -----------------------------------------------------------
  // Ensure device commands are in alphabetical order
  // -----------------------------------------------------------
  init_device_commands();

  // -----------------------------------------------------------
  // Read saved LED/WiFi settings
  // -----------------------------------------------------------
  get_nvs_led_info();

  // -----------------------------------------------------------
  // Configure dynamic frequency scaling
  // -----------------------------------------------------------
#ifdef CONFIG_PM_ENABLE
  esp_pm_config_esp32_t pm_config = {
    .max_freq_mhz = ESP_PM_CPU_FREQ_MAX,
    //.min_freq_mhz = (int) rtc_clk_xtal_freq_get(),
    .min_freq_mhz = esp_clk_xtal_freq() / 1000000,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    .light_sleep_enable = true
#else
    .light_sleep_enable = false
#endif
  };
  err = esp_pm_configure(&pm_config);
  if ( err != ESP_OK )
  {
    F_LOGE(true, true, LC_YELLOW, "Problem running esp_pm_configure()");
  }
#endif // CONFIG_PM_ENABLE

  // -----------------------------------------------------------
  // setup configuration
  // -----------------------------------------------------------
  static Configuration_List_t cfg_list[2];
  cfg_list[0] = init_mqtt_config();
  _config_build_list(cfg_list, 1);

  // -----------------------------------------------------------
  // Display some useful info on boot
  // -----------------------------------------------------------
  print_app_info();
#if defined(CONFIG_DEBUG)
  show_nvs_usage();
  printHeapInfo();
#endif

  // -----------------------------------------------------------
  // Share this between WiFi & HTTP
  // ------------------ -----------------------------------------
  static httpd_handle_t httpServer = NULL;

  // -----------------------------------------------------------
  // WiFi Initialisation
  // -----------------------------------------------------------
  init_wifi(&httpServer);

  // -----------------------------------------------------------
  // HTTP Initialisation
  // -----------------------------------------------------------
  init_http_server(&httpServer);

  // -----------------------------------------------------------
  // MQTT Initialisation
  // -----------------------------------------------------------
  init_mqtt_client();
#if CONFIG_USE_BOTH_CORES
  xTaskCreate(mqtt_send_task, TASK_NAME_MQTT_BROKER, STACKSIZE_MQTT_BROKER, NULL, TASK_PRIORITY_MQTT, &xMQTTHandle);
#else
  xTaskCreatePinnedToCore(mqtt_send_task, TASK_NAME_MQTT_BROKER, STACKSIZE_MQTT_BROKER, NULL, TASK_PRIORITY_MQTT, &xMQTTHandle, TASKS_CORE);
#endif /* CONFIG_USE_BOTH_CORES */

  // -----------------------------------------------------------
  // Random CCTV Movement
  // -----------------------------------------------------------
#if defined(CONFIG_CCTV)
#if CONFIG_USE_BOTH_CORES
  xTaskCreate(cctv_task, TASK_NAME_CCTV, STACKSIZE_CCTV, NULL, TASK_PRIORITY_CCTV, &xCCTVHandle);
#else
  xTaskCreatePinnedToCore(cctv_task, TASK_NAME_CCTV, STACKSIZE_CCTV, NULL, TASK_PRIORITY_CCTV, &xCCTVHandle, TASKS_CORE);
#endif /* CONFIG_USE_BOTH_CORES */
#endif /* CONFIG_CCTV */

  // -----------------------------------------------------------
  // Set a random pattern at boot time.
  // -----------------------------------------------------------
  lights_dailies();

  // -----------------------------------------------------------
  // Begin the light display task
  // -----------------------------------------------------------
  if ( lights_init() == ESP_OK )
  {
#if CONFIG_USE_BOTH_CORES
    xTaskCreate(lights_task, TASK_NAME_LIGHTS, STACKSIZE_LIGHTS, NULL, TASK_PRIORITY_LIGHTS, &xLightsHandle);
#else
    xTaskCreatePinnedToCore(lights_task, TASK_NAME_LIGHTS, STACKSIZE_LIGHTS, NULL, TASK_PRIORITY_LIGHTS, &xLightsHandle, LIGHTS_CORE);
#endif /* CONFIG_USE_BOTH_CORES */
  }

  // -----------------------------------------------------------
  // Initialise gpio pins and task if applicable
  // -----------------------------------------------------------
  init_gpio_pins(&control_vars);
#if CONFIG_MOTION_DETECTION
#if CONFIG_USE_BOTH_CORES
  xTaskCreate(gpio_task, TASK_NAME_GPIO, STACKSIZE_GPIO, NULL, TASK_PRIORITY_GPIO, &xGpioHandle);
#else
  xTaskCreatePinnedToCore(gpio_task, TASK_NAME_GPIO, STACKSIZE_GPIO, NULL, TASK_PRIORITY_GPIO, &xGpioHandle, TASKS_CORE);
#endif /* CONFIG_USE_BOTH_CORES */
#endif /* CONFIG_MOTION_DETECTION */

  // -----------------------------------------------------------
  // Initialise schedule management
  // -----------------------------------------------------------
  init_scheduler();
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  const esp_timer_create_args_t scheduler_args = {
      .callback = &scheduler_task,
      .name = "scheduler"};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour
  esp_timer_handle_t schedulerHandle;
  esp_timer_create(&scheduler_args, &schedulerHandle);
  esp_timer_start_periodic(schedulerHandle, 1000000);

  // -----------------------------------------------------------
  // Are we showing runtime stats?
  // -----------------------------------------------------------
#if (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
#if CONFIG_USE_BOTH_CORES
  xTaskCreate(stats_task, TASK_NAME_STATS, STACKSIZE_STATS, NULL, TASK_PRIORITY_STATS, &xStatsHandle);
#else
  xTaskCreatePinnedToCore(stats_task, TASK_NAME_STATS, STACKSIZE_STATS, NULL, TASK_PRIORITY_STATS, &xStatsHandle, TASKS_CORE);
#endif /* CONFIG_USE_BOTH_CORES */
#endif

  // -----------------------------------------------------------
  // Clear pause request for boot initialisation
  // -----------------------------------------------------------
  lightsUnpause(PAUSE_BOOT, true);

  // -----------------------------------------------------------
  // Did we just do an OTA update?
  // -----------------------------------------------------------
  new_fw_delay();

  // -----------------------------------------------------------
  // Inform we have completed initialisation.
  // -----------------------------------------------------------
  F_LOGI(true, true, LC_GREY, "Main task thread finished");
}

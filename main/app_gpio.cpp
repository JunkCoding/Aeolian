



#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include "freertos/queue.h"
#include <driver/gpio.h>

#include "app_main.h"
#include "app_gpio.h"
#include "device_control.h"
#include "app_utils.h"

#if defined (CONFIG_MOTION_DETECTION)
QueueHandle_t gpio_evt_queue = NULL;
IRAM_ATTR static void gpio_isr_handler (void* arg)
{
  uint8_t zone = 0;
  uint32_t pin = (uint32_t)arg;
  bool state = gpio_get_level (pin);

  switch (pin)
  {
    case GPIO_INPUT_0:
      zone = ZONE_1;
      break;
    case GPIO_INPUT_1:
      zone = ZONE_2;
      break;
    case GPIO_INPUT_2:
      zone = ZONE_3;
      break;
    case GPIO_INPUT_3:
      zone = ZONE_4;
      break;
    default:
      return;
      break;
  }

  if (overlay[zone].state == state)
  {
    return;
  }

  overlay[zone].state = state;

  zone_event_t event = {zone = zone, state = state};

  xQueueSendFromISR (gpio_evt_queue, &event, NULL);
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void gpio_task (void *arg)
#else
void gpio_task (void *arg)
#endif
{
  zone_event_t event;

  CHECK_ERROR_CODE (esp_task_wdt_add (NULL), ESP_OK);
  CHECK_ERROR_CODE (esp_task_wdt_status (NULL), ESP_OK);

  for (;;)
  {
    if (xQueueReceive (gpio_evt_queue, &event, 1000) == pdTRUE)
    {
      if (event.state)
      {
        if (event.zone == ZONE_1)
        {
          // flood[SECURITY_LIGHT].timer = FLOOD_TIMER;
        }

        setZone (event.zone, OL_MASK_NIGHTTIME, CRGBMediumWhite, OVERLAY_TIMER);
      }
    }

    CHECK_ERROR_CODE (esp_task_wdt_reset (), ESP_OK);
  }
}

esp_err_t init_gpio_input (gpio_num_t gpio_num)
{
  esp_err_t err = ESP_OK;

  F_LOGI(true, true, LC_GREY, "init_gpio_input(%d)", gpio_num);

  // dac disable
  if (gpio_num == 25)
  {
    F_LOGI(true, true, LC_GREY, "GPIO is DAC Channel 1");

    err = dac_output_disable (DAC_CHANNEL_1);
  }
  else if (gpio_num == 26)
  {
    F_LOGI(true, true, LC_GREY, "GPIO is DAC Channel 2");

    err = dac_output_disable (DAC_CHANNEL_2);
  }

  if (err == ESP_OK)
  {
    if (RTC_GPIO_IS_VALID_GPIO (gpio_num))
    {
      F_LOGI(true, true, LC_GREY, "GPIO is RTC");

      err = rtc_gpio_deinit (gpio_num);
      err = rtc_gpio_set_direction (gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
      err = rtc_gpio_pulldown_en (gpio_num);
      err = rtc_gpio_pullup_dis (gpio_num);
    }
    else
    {
      F_LOGI(true, true, LC_GREY, "Standard GPIO");

      err = gpio_set_direction (gpio_num, GPIO_MODE_INPUT);
      err = gpio_pulldown_en (gpio_num);
      err = gpio_pullup_dis (gpio_num);
    }

    // Doesn't work without this line (copied from gpio.c)
    PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG[gpio_num]);

    // GPIO inputs (for motion detection)
    // GPIO_INTR_DISABLE. GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE
    // GPIO_INTR_ANYEDGE, GPIO_INTR_LOLEVEL, GPIO_INTR_HILEVEL
    // *****************************************
    err = gpio_set_intr_type (gpio_num, GPIO_INTR_ANYEDGE);
    err = gpio_isr_handler_add (gpio_num, gpio_isr_handler, (void*)gpio_num);
    err = gpio_intr_enable (gpio_num);
  }
  else
  {
    F_LOGE(true, true, LC_YELLOW, "Error enabling GPIO");
  }

  return err;
}
#endif /* CONFIG_MOTION_DETECTION */

esp_err_t init_gpio_pins(controlvars_t *control_vars)
{
  // -----------------------------------------------------------
  // GPIO outputs (for security lights)
  // -----------------------------------------------------------
  gpio_config_t io_conf;
  io_conf.mode         = GPIO_MODE_OUTPUT;                             // Output
  io_conf.intr_type    = GPIO_INTR_DISABLE;                            // Disable interrupt trigger
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;                        // Disable pulldown resistor
  io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;                          // Disable pullup resistor
#if defined (CONFIG_AEOLIAN_DEV_ROCKET)
  io_conf.pin_bit_mask = (ROCKET_SETUP);
  gpio_config (&io_conf);
  gpio_set_drive_capability ((gpio_num_t)ROCKET_ONE, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability ((gpio_num_t)ROCKET_TWO, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability ((gpio_num_t)ROCKET_THREE, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability ((gpio_num_t)ROCKET_FOUR, GPIO_DRIVE_CAP_1);
  // Configure power switch, first
  //gpio_set_level (ROCKET_POWER, 0);
  // Configure launch switches
  gpio_set_level ((gpio_num_t)ROCKET_ONE, ROCKET_SAFE);
  gpio_set_level ((gpio_num_t)ROCKET_TWO, ROCKET_SAFE);
  gpio_set_level ((gpio_num_t)ROCKET_THREE, ROCKET_SAFE);
  gpio_set_level ((gpio_num_t)ROCKET_FOUR, ROCKET_SAFE);
#endif
  io_conf.pin_bit_mask = (1ULL << control_vars->light_gpio_pin | 1ULL << 2);
  gpio_config (&io_conf);
  gpio_set_drive_capability ((gpio_num_t)control_vars->light_gpio_pin, GPIO_DRIVE_CAP_1);

#if CONFIG_MOTION_DETECTION
  // -----------------------------------------------------------
  // Install gpio isr service
  // -----------------------------------------------------------
  gpio_install_isr_service (ESP_INTR_FLAG_DEFAULT);

  // -----------------------------------------------------------
  // Create a queue to handle gpio event from isr
  // -----------------------------------------------------------
  gpio_evt_queue = xQueueCreate(10, sizeof (zone_event_t));

  // -----------------------------------------------------------
  // Trigger input pins
  // -----------------------------------------------------------
  init_gpio_input (GPIO_INPUT_0);
  init_gpio_input (GPIO_INPUT_1);
  init_gpio_input (GPIO_INPUT_2);
  init_gpio_input (GPIO_INPUT_3);
#endif /* CONFIG_MOTION_DETECTION */

  return ESP_OK;
}

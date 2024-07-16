#ifndef  __APP_GPIO_H__
#define  __APP_GPIO_H__

#include "app_lightcontrol.h"

// Inputs for the motion sensors
#define GPIO_INPUT_0     CONFIG_MOTION_INPUT_1
#define GPIO_INPUT_1     CONFIG_MOTION_INPUT_2
#define GPIO_INPUT_2     CONFIG_MOTION_INPUT_3
#define GPIO_INPUT_3     CONFIG_MOTION_INPUT_4
#define GPIO_INPUT_PINS  ((1ULL<<GPIO_INPUT_0) | (1ULL<<GPIO_INPUT_1) | (1ULL<<GPIO_INPUT_2) | (1ULL<<GPIO_INPUT_3))
#define ESP_INTR_FLAG_DEFAULT 0

#if CONFIG_MOTION_DETECTION
void      gpio_task (void *);
#endif /* CONFIG_MOTION_DETECTION */

esp_err_t init_gpio_pins (controlvars_t *);

#endif /* __GPIO_H__ */
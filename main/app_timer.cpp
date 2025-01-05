

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/timer.h>
#include <freertos/event_groups.h>

#include "app_main.h"
#include "app_timer.h"
#include "app_utils.h"

timer_info_t *_timer_init(int group, int timer_idx, uint16_t divider, uint16_t interval);

#define   USE_ISR_CALLBACK    true

/* Timer interrupt service routine */
#ifdef USE_ISR_CALLBACK
IRAM_ATTR static bool timer_callback_isr ( void *args )
{
  BaseType_t mustYield = pdFALSE;
  timer_info_t *info = (timer_info_t *)args;

  /*
  uint64_t alarm_counter = timer_group_get_counter_value_in_isr (info->timer_group, info->timer_idx);
  if ( !info->auto_reload )
  {
    timer_group_set_alarm_value_in_isr(info->timer_group, info->timer_idx, alarm_counter + info->alarm_value);
  }*/

  if ( info->taskHandle != NULL )
  {
    //vTaskNotifyGiveFromISR ( info->taskHandle, &mustYield );
    //xTaskNotifyFromISR ( info->taskHandle, alarm_counter, eSetValueWithOverwrite, &mustYield );
    xTaskNotifyFromISR ( info->taskHandle, xPortGetCoreID(), eSetValueWithOverwrite, &mustYield );
  }
  else if ( info->semaphore != NULL )
  {
    xSemaphoreGiveFromISR ( info->semaphore, &mustYield );
  }

  return mustYield == pdTRUE;
}
#else
IRAM_ATTR static void timer_isr ( void *args )
{
  BaseType_t mustYield = pdFALSE;
  timer_info_t *info = (timer_info_t *)args;
  int curcore = xPortGetCoreID ();

  //uint32_t intr = timer_group_get_intr_status_in_isr ( info->timer_group );
  //uint64_t cval = timer_group_get_counter_value_in_isr ( info->timer_group, info->timer_idx );

  //timer_group_clr_intr_status_in_isr(info->timer_group, intr);

  /* Unblock the waiting task */
  if ( info->taskHandle != NULL )
  {
    vTaskNotifyGiveFromISR ( info->taskHandle, &mustYield );
  }
  else if ( info->semaphore != NULL )
  {
    xSemaphoreGiveFromISR ( info->semaphore, &mustYield );
  }

  if ( mustYield  != pdFALSE )
  {
    //portYIELD_FROM_ISR ( mustYield );
    //ets_printf("isr: %d\n", info->timer_idx);
  }

  //ets_printf ( "%d, %d, %d\n", intr, info->timer_group, info->timer_idx );
  timer_group_clr_intr_status_in_isr ( info->timer_group, info->timer_idx );
  timer_group_enable_alarm_in_isr ( info->timer_group, info->timer_idx );
}
#endif

timer_info_t *_timer_init ( timer_group_t group, timer_idx_t timer_idx, uint16_t divider, uint16_t interval )
{
  timer_config_t config = {
    .alarm_en              = TIMER_ALARM_EN,          // Alarm Enable
    .counter_en            = TIMER_PAUSE,             // If the counter is enabled it will start incrementing / decrementing immediately after calling timer_init()
    .intr_type             = TIMER_INTR_LEVEL,        // Is interrupt triggered on timer’s alarm (timer_intr_mode_t)
    .counter_dir           = TIMER_COUNT_UP,          // Does counter increment or decrement (timer_count_dir_t)
    .auto_reload           = TIMER_AUTORELOAD_EN,     // If counter should auto_reload a specific initial value on the timer’s alarm, or continue incrementing or decrementing.
    .clk_src               = TIMER_SRC_CLK_DEFAULT,   // Use default clock source
    .divider               = divider,                 // Divisor of the incoming 80 MHz (12.5nS) APB_CLK clock. E.g. 80 = 1uS per timer tick
  };

  /* Configure timer */
  timer_init ( group, timer_idx, &config );

  /* Load counter value */
  timer_set_counter_value ( group, timer_idx, 0 );

  /* Configure the alarm value and the interrupt on alarm. */
  timer_set_alarm_value ( group, timer_idx, (divider == TIMER_DIVIDER_MS) ? interval * 10 : interval );

  timer_info_t *timer_info = (timer_info_t *)pvPortMalloc(sizeof(timer_info_t));
  timer_info->timer_group  = group;
  timer_info->timer_idx    = timer_idx;
  timer_info->auto_reload  = TIMER_AUTORELOAD_EN;
  timer_info->alarm_value  = (divider == TIMER_DIVIDER_MS) ? interval * 10 : interval;

#ifdef USE_ISR_CALLBACK
  timer_isr_callback_add(group, timer_idx, timer_callback_isr, timer_info, 0);
#else
  timer_isr_register ( group, timer_idx, timer_isr, timer_info, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED, NULL );
  //timer_isr_register ( group, timer_idx, timer_isr, timer_info, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED, NULL );
#endif
  return timer_info;
}
void timer_init_with_semaphore ( timer_group_t group, timer_idx_t timer_idx, uint16_t divider, uint16_t interval, SemaphoreHandle_t handle )
{
  timer_info_t *timer_info = _timer_init ( group, timer_idx, divider, interval );
  timer_info->semaphore    = handle;
  timer_info->taskHandle   = NULL;
  timer_enable_intr ( group, timer_idx );
  timer_start ( group, timer_idx );
}
void timer_init_with_taskHandle ( timer_group_t group, timer_idx_t timer_idx, uint16_t divider, uint16_t interval, TaskHandle_t handle )
{
  timer_info_t *timer_info = _timer_init ( group, timer_idx, divider, interval );
  timer_info->semaphore    = NULL;
  timer_info->taskHandle   = handle;
  timer_enable_intr ( group, timer_idx );
  timer_start ( group, timer_idx );
}

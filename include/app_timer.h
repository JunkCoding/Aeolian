#ifndef __APP_TIMER_H__
#define __APP_TIMER_H__

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/timer.h>
#include <freertos/event_groups.h>

#define TIMER_DIVIDER_US   80
#define TIMER_DIVIDER_MS   8000

typedef struct
{
  int                 timer_group;
  int                 timer_idx;
  timer_autoreload_t  auto_reload;
  uint64_t            alarm_value;
  SemaphoreHandle_t   semaphore;
  TaskHandle_t        taskHandle;
} timer_info_t;

void timer_init_with_taskHandle (timer_group_t group, timer_idx_t timer_idx, uint16_t divider, uint interval, TaskHandle_t handle);
void timer_init_with_semaphore  (timer_group_t group, timer_idx_t timer_idx, uint16_t divider, uint interval, SemaphoreHandle_t handle);

#endif
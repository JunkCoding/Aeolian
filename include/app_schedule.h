#ifndef   __SCHEDULE_H__
#define   __SCHEDULE_H__

#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>

#include "app_lightcontrol.h"

typedef enum
{
  // Lower nibble for integers/bitflags
  EVENT_LIGHTSON      = 0x01,  // Event forces lights to be on during period or whole day
  EVENT_LIGHTSOFF     = 0x02,  // Event forces lights to be off during period or whole day
  // Higher nibble for bitflags
  EVENT_AUTONOMOUS    = 0x10,  // Event will display with sunset/sunrise switching
  EVENT_DEFAULT       = 0x20,  // Default event to be run when no other event is matched
  EVENT_IMMUTABLE     = 0x40,  // Event cannot be removed/altered
  EVENT_INACTIVE      = 0x80,  // Event is not active (don't run)
} schedulerFlags_t;

typedef struct
{
  uint8_t   hour;
  uint8_t   minute;
} hm_t;

// NVS Keu => hh mm dd
typedef struct
{
  hm_t              start;        //
  hm_t              end;          //
  uint8_t           day;          // Day of the week
  uint8_t           theme;        // Theme
  led_brightness_t  dim;          // Default brightness
  uint8_t           flags;        // Stuff
} weekly_event_t;

// NVS Key => hh mm DD MM
typedef struct
{
  hm_t              start;        //
  hm_t              end;          //
  uint8_t           dayStart;     //
  uint8_t           dayEnd;       //
  uint8_t           month;        //
  uint8_t           theme;        //
  led_brightness_t  dim;          // Default brightness
  uint8_t           flags;        //
} annual_event_t;

void      reboot (void);
esp_err_t init_scheduler (void);
//void    scheduler_task(void* pvParameters);
void      scheduler_task (void *arg);

#endif /* __SCHEDULE_H__ */

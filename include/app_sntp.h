#ifndef  __SNTP_H__
#define  __SNTP_H__

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t sntp_mutex;

uint64_t       getTicks_base();
void           setTicks_base( uint64_t ticks_base );
uint64_t       mp_hal_ticks_ms( void );
uint64_t       mp_hal_ticks_us( void );

void           init_sntp(void);
void           obtain_time(void);
bool           isTimeSynced(void);

#endif

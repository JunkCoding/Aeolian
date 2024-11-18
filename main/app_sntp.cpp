

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <driver/gpio.h>

#include <soc/rmt_struct.h>

#include <lwip/ip_addr.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>

#include "app_main.h"
#include "app_sntp.h"
#include "app_utils.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

bool ntp_time_sync = false;
struct timeval last_sync = {};

RTC_DATA_ATTR uint64_t mp_hal_ticks_base;

WORD_ALIGNED_ATTR DRAM_ATTR SemaphoreHandle_t sntp_mutex = NULL;

IRAM_ATTR bool isTimeSynced ()
{
  return ntp_time_sync;
}

IRAM_ATTR void time_sync_notification_cb ( struct timeval *tv )
{
  ntp_time_sync = true;
  gettimeofday ( &last_sync, NULL );
  F_LOGI(true, true, LC_GREEN, "Time synchronization event");
}

//------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint64_t getTicks_base ()
#else
uint64_t getTicks_base ()
#endif
{
  uint64_t ticks_base = 0;
  if ( sntp_mutex )
  {
    if ( xSemaphoreTake ( sntp_mutex, 1000 / portTICK_PERIOD_MS ) == pdTRUE )
    {
      ticks_base = mp_hal_ticks_base;
      xSemaphoreGive ( sntp_mutex );
    }
  }
  else
  {
    ticks_base = mp_hal_ticks_base;
  }
  return ticks_base;
}

//-------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void setTicks_base (uint64_t ticks_base)
#else
void setTicks_base (uint64_t ticks_base)
#endif
{
  if ( sntp_mutex )
  {
    if ( xSemaphoreTake ( sntp_mutex, 1000 / portTICK_PERIOD_MS ) == pdTRUE )
    {
      mp_hal_ticks_base = ticks_base;
      xSemaphoreGive ( sntp_mutex );
    }
  }
  else
  {
    mp_hal_ticks_base = ticks_base;
  }
}

//------------------------------
IRAM_ATTR uint64_t mp_hal_ticks_ms ( void )
{
  struct timeval tv;
  gettimeofday ( &tv, NULL );
  return ((((uint64_t)tv.tv_sec * 1000000) + (uint64_t)tv.tv_usec) - getTicks_base ()) / 1000;
}

//------------------------------
IRAM_ATTR uint64_t mp_hal_ticks_us ( void )
{
  struct timeval tv;
  gettimeofday ( &tv, NULL );
  return ((((uint64_t)tv.tv_sec * 1000000) + (uint64_t)tv.tv_usec) - getTicks_base ());
}

void sntp_setTimeZone ( const char *tz_string )
{
  // Set the timezone
  // *****************************************
  //setenv("TZ", "Europe/London,M3.5.0/1,M10.5.0/2", 1);
  //setenv("TZ", "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00", 1);
  setenv ( "TZ", tz_string, true );
  tzset ();
}

#if defined (CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM)
void sntp_sync_time ( struct timeval *tv )
{
  settimeofday ( tv, NULL );
  F_LOGI(true, true, LC_YELLOW, "Time is synchronized from custom code");
  sntp_set_sync_status ( SNTP_SYNC_STATUS_COMPLETED );
}
#endif

void init_sntp ( void )
{
  F_LOGI(true, true, LC_GREEN, "Initialising SNTP...");

  // Set time to '0' (aka Thur. 1st Jan. 1970)
  struct timeval zeroTime = {};
  settimeofday ( &zeroTime, NULL );

  // ----------------------------------
  // Set the timezone
  // ----------------------------------
  sntp_setTimeZone ( "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00" );

#ifdef LWIP_DHCP_GET_NTP_SRV
  /**
   * NTP server address could be aquired via DHCP,
   * see following menuconfig options:
   * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
   * 'LWIP_SNTP_DEBUG' - enable debugging messages
   *
   * NOTE: This call should be made BEFORE esp aquires IP address from DHCP,
   * otherwise NTP option would be rejected by default.
   */
  esp_sntp_servermode_dhcp(1);      // accept NTP offers from DHCP server, if any
#endif
}

void obtain_time ( void )
{
  // Stop SNTP if enabled
  if ( esp_sntp_enabled() )
  {
    esp_sntp_stop();
  }

  // Flag as not synced
  ntp_time_sync = false;

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

#if LWIP_DHCP_GET_NTP_SRV && SNTP_MAX_SERVERS > 1
  esp_sntp_setservername(1, "0.pool.ntp.org");
#elif defined CONFIG_SNTP_TIME_SERVER
  esp_sntp_setservername(0, CONFIG_SNTP_TIME_SERVER);
#endif

  sntp_set_time_sync_notification_cb ( time_sync_notification_cb );

  // Start SNTP
  esp_sntp_init();

  F_LOGI(true, true, LC_GREY, "List of configured NTP servers:");

  for ( uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i )
  {
    if ( esp_sntp_getservername(i) )
    {
      F_LOGI(true, true, LC_GREY, "Server %d: %s", i, esp_sntp_getservername(i));
    }
    else
    {
         // we have either IPv4 or IPv6 address, let's print it
      char buff[INET6_ADDRSTRLEN];
      ip_addr_t const *ip = esp_sntp_getserver(i);
      if ( ipaddr_ntoa_r ( ip, buff, INET6_ADDRSTRLEN ) != NULL )
      {
        F_LOGI(true, true, LC_GREY, "Server %d: %s", i, buff);
      }
    }
  }
}

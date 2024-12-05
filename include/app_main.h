#ifndef   __MAIN_H__
#define   __MAIN_H__

#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include <esp_sntp.h>

#include "app_task.h"
#include "ws2812_driver.h"


// Set our location
#define     LATITUDE                  +51
#define     LONGITUDE                 -3.10

#define     TASKS_CORE                0
#define     LIGHTS_CORE               1

#define     TWDT_TIMEOUT_S            5
#define     TASK_RESET_PERIOD_S       2
#define     MAX_HTTPD_CLIENTS         12          // Too small and experience difficulties accessing the web page
                                                  // Fixme: Make the websockets more dynamic
#define     URI_DECODE_BUFLEN         128

// Where can we find the LED strip?
#define     WS2812_PIN                CONFIG_WS2812_PIN
#define     LIGHT_PIN                 CONFIG_LIGHT_PIN

typedef enum
{
  SECURITY_LIGHT
} light_t;
typedef enum
{
  ZONE_1, ZONE_2, ZONE_3, ZONE_4
} zone_t;

#define S(str) (str == NULL ? "<null>": str)

/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define BINARY_PATTERN_INT16      BINARY_PATTERN_INT8 BINARY_PATTERN_INT8
#define BYTE_TO_BINARY_INT16(i)   BYTE_TO_BINARY_INT8((i) >> 8), BYTE_TO_BINARY_INT8(i)

#define BINARY_PATTERN_INT32      BINARY_PATTERN_INT16 BINARY_PATTERN_INT16
#define BYTE_TO_BINARY_INT32(i)   BYTE_TO_BINARY_INT16((i) >> 16), BYTE_TO_BINARY_INT16(i)

#define BINARY_PATTERN_INT64      BINARY_PATTERN_INT32 BINARY_PATTERN_INT32
#define BYTE_TO_BINARY_INT64(i)   BYTE_TO_BINARY_INT32((i) >> 32), BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */

// Automated light on/off time
#define WAKE_TIME                   (07 * 60) + 00                             // 07:00
#define SLEEP_TIME                  (21 * 60) + 30                             // 21:30

// Dim lights at this time
#define DIM_TIME                    (21 * 60) + 00                             // 21:00
#define WIFI_CONNECTED_BIT          BIT0

#define NVS_PARTITION               "nvs"
#define NVS_LIGHT_ZONES             "light_zones"
#define NVS_LIGHT_CONFIG            "light_config"
#define NVS_WIFI_SETTINGS           "wifi_settings"
#define NVS_WIFI_AP_CFG             "wifi_settings"
#define NVS_WIFI_STA_CFG            "wifi_settings"
#define NVS_MQTT_CONFIG             "mqtt_settings"
#define NVS_WEEKLY_SCHEDULE         "sched_weekly"
#define NVS_ANNUAL_SCHEDULE         "sched_annual"
#define NVS_SYS_INFO                "sysinfo"

#define NVS_OTA_UPDATE_KEY          "ota_update"
#define NVS_BOOT_COUNT_KEY          "boot_count"
#define NVS_HOSTNAME_KEY            "hostname"

#define JSON_RESPONSE_AS_INTVAL     "{\"%s\": %d}"
#define JSON_RESPONSE_AS_STRVAL     "{\"%s\": %s}"
#define JSON_RESPONSE_AS_STRING     "{\"%s\": \"%s\"}"

#define JSON_STR_CMD                "cmd"
#define JSON_STR_ID                 "id"
#define JSON_STR_ARG                "arg"
#define JSON_STR_REASON             "reason"
#define JSON_STR_THEME              "theme"
#define JSON_STR_PATTERN            "pattern"
#define JSON_STR_DELAY_US           "delay"
#define JSON_STR_SYNC               "sync"
#define JSON_STR_SYNC_VER           "a"

// Sync packet
#define JSON_SYNC_STR               "{\"" JSON_STR_SYNC "\": [\"" JSON_STR_SYNC_VER "\",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\"]}"

#define JSON_LEDCMD_2_INT           "{\"" JSON_STR_CMD "\": {\"" JSON_STR_ID "\": %d, \"" JSON_STR_ARG "\": %d}}"
#define JSON_LEDCMD_2_STR           "{\"" JSON_STR_CMD "\": {\"" JSON_STR_ID "\": %d, \"" JSON_STR_ARG "\": \"%s\"}}"
#define JSON_RGBCOL_STR             "{\"" JSON_STR_CMD "\": {\"" JSON_STR_ID "\": %d, \"" JSON_STR_ARG "\": \"%02x%02x%02x\"}}"

#define JSON_SWITCHOFF_STR          "{\"" JSON_STR_CMD "\": {\"" JSON_STR_ID "\": %d, \"" JSON_STR_ARG "\": \"%s\", \"reason\": %d}}"
#define JSON_SWITCHON_STR           "{\"" JSON_STR_CMD "\": {\"" JSON_STR_ID "\": %d, \"" JSON_STR_ARG "\": \"%s\", \"reason\": %d}, \"delay\": %d, \"theme\": %d, \"pattern\": %d}"

#define JSON_SUCCESS_STR            "\n\t\"success\": true\n}"
#define JSON_FAILURE_STR            "\n\t\"success\": false\n}"
#define JSON_REBOOT_STR             "{\n\t\"message\": \"Rebooting...\",\n\t\"success\": true\n}"

#if not defined CONFIG_AEOLIAN_DEBUG_DEV
#if defined (CONFIG_ESP_GDBSTUB_ENABLED)
#pragma once
#warning GDBSTUB enabled
#endif /* CONFIG_ESP_GDBSTUB_ENABLED */
#endif /* CONFIG_AEOLIAN_DEBUG_DEV */

/*
 * Macro to check the outputs of TWDT functions and trigger an abort if an
 * incorrect code is returned.
 */
#define CHECK_ERROR_CODE(returned, expected) ({                        \
            if(returned != expected){                                  \
                printf("TWDT ERROR\n");                                \
                abort();                                               \
            }                                                          \
})
typedef enum
{
  SCHEDULE_DISABLED = 0x01
} schedule_mask;

typedef struct
{
  time_t    nextcheck;
  time_t    ts_sunrise;
  time_t    ts_sunset;
  uint32_t  min_sunrise;
  uint32_t  min_sunset;
  uint16_t  lux_level;
  bool      isdark;
} lightcheck_t;

extern uint32_t boot_count, ota_update;
extern lightcheck_t lightcheck;

void set_light (uint8_t id, uint8_t mode, uint8_t time);
bool lightStatus (uint8_t id);
bool zoneStatus (uint8_t id);
void setZone (uint8_t id, uint8_t mask, cRGB color, uint16_t duration);

#endif

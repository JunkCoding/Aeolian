
#define _PRINT_CHATTY
#define V_HEAP_INFO

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include <esp_http_server.h>
#include <esp32/rom/rtc.h>
#include <esp_system.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <esp_mac.h>

#include <driver/gpio.h>

#include <lwip/err.h>

#include <nvs_flash.h>

#include "app_main.h"
#include "app_utils.h"
#include "app_flash.h"
#include "app_lightcontrol.h"
#include "app_wifi.h"
#include "app_httpd.h"
#include "device_control.h"
#include "moonphase.h"
#include "app_mqtt.h"
#include "app_mqtt_settings.h"

#define   BUF_SIZE    512

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;
extern themes_t themes[];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct rst_info *rst_info_t;
extern rst_info_t sys_rst_info;

extern time_t build_time;       // user_main.c
extern const uint32_t build_number; // user_main.c

const char *print_reset_reason (RESET_REASON reason)
{
  char *reasonString;

  switch ( (uint16_t)reason )
  {
    case 0:
      reasonString = (char *)"ESP_RST_UNKNOWN";
      break;                                                /**<0,  Reset reason can not be determined */
    case 1:
      reasonString = (char *)"ESP_RST_POWERON";
      break;                                                /**<1,  Reset due to power-on event */
    case 3:
      reasonString = (char *)"ESP_RST_EXT";
      break;                                                /**<3,  Reset by external pin (not applicable for ESP32) */
    case 4:
      reasonString = (char *)"ESP_RST_SW";
      break;                                                /**<4,  Software reset via esp_restart */
    case 5:
      reasonString = (char *)"ESP_RST_PANIC";
      break;                                                /**<5,  Software reset due to exception/panic */
    case 6:
      reasonString = (char *)"ESP_RST_INT_WDT";
      break;                                                /**<6,  Reset (software or hardware) due to interrupt watchdog */
    case 7:
      reasonString = (char *)"ESP_RST_TASK_WDT";
      break;                                                /**<7,  Reset due to task watchdog */
    case 8:
      reasonString = (char *)"ESP_RST_WDT";
      break;                                                /**<8,  TReset due to other watchdogs */
    case 9:
      reasonString = (char *)"ESP_RST_DEEPSLEEP";
      break;                                                /**<9,  Reset after exiting deep sleep mode */
    case 10:
      reasonString = (char *)"ESP_RST_BROWNOUT";
      break;                                                /**<10, Brownout reset (software or hardware) */
    case 11:
      reasonString = (char *)"ESP_RST_SDIO";
      break;                                                /**<11, TReset over SDIO */
    case 12:
      reasonString = (char *)"ESP_RST_USB";
      break;                                                /**<12, Reset by USB peripheral */
    case 13:
      reasonString = (char *)"ESP_RST_JTAG";
      break;                                                /**<13, Reset by JTAG */
    case 14:
      reasonString = (char *)"ESP_RST_EFUSE";
      break;                                                /**<14, Reset due to efuse error */
    case 15:
      reasonString = (char *)"ESP_RST_PWR_GLITCH";
      break;                                                /**<15, Reset due to power glitch detected */
    case 16:
      reasonString = (char *)"ESP_RST_CPU_LOCKUP";
      break;                                                /**<16, Reset due to CPU lock up */
    default:
      reasonString = (char *)"NO_MEAN";
  }

  return reasonString;
}

const char *verbose_print_reset_reason (RESET_REASON reason)
{
  char *reasonString;

  switch ( (uint16_t)reason )
  {
    case 1:
      reasonString = (char *)"Vbat power on reset";
      break;
    case 3:
      reasonString = (char *)"Software reset digital core";
      break;
    case 4:
      reasonString = (char *)"Legacy watch dog reset digital core";
      break;
    case 5:
      reasonString = (char *)"Deep Sleep reset digital core";
      break;
    case 6:
      reasonString = (char *)"Reset by SLC module, reset digital core";
      break;
    case 7:
      reasonString = (char *)"Timer Group0 Watch dog reset digital core";
      break;
    case 8:
      reasonString = (char *)"Timer Group1 Watch dog reset digital core";
      break;
    case 9:
      reasonString = (char *)"RTC Watch dog Reset digital core";
      break;
    case 10:
      reasonString = (char *)"Instrusion tested to reset CPU";
      break;
    case 11:
      reasonString = (char *)"Time Group reset CPU";
      break;
    case 12:
      reasonString = (char *)"Software reset CPU";
      break;
    case 13:
      reasonString = (char *)"RTC Watch dog Reset CPU";
      break;
    case 14:
      reasonString = (char *)"for APP CPU, reset by PRO CPU";
      break;
    case 15:
      reasonString = (char *)"Reset when the vdd voltage is not stable";
      break;
    case 16:
      reasonString = (char *)"RTC Watch dog reset digital core and rtc module";
      break;
    default:
      reasonString = (char *)"NO_MEAN";
  }

  return reasonString;
}

// ***********************************************
int _set_led_control (char *buf, int bufsize, char *param, char *value, int xtype)
{
  esp_err_t err = save_nvs_num ((const char *)NVS_LIGHT_CONFIG, (const char *)param, (char*) value, (tNumber)xtype);
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_STRVAL, param, err == ESP_OK?value:"false"));
}

// ***********************************************
#if defined (CONFIG_AEOLIAN_DEV_ROCKET)
// Handle here to prevent overlapping timers (lazy coding)
esp_timer_handle_t reset_task_handle = NULL;

rocketList_t rockets[] = {
  {ROCKET_ONE,    ROCKET_SAFE},
  {ROCKET_TWO,    ROCKET_SAFE},
  {ROCKET_THREE,  ROCKET_SAFE},
  {ROCKET_FOUR,   ROCKET_SAFE}
};
#define RLTOP ((sizeof(rockets) / sizeof(rocketList_t)) -1)

// Turn the power off after a given period
static void rocket_reset_callback (void *arg)
{
  uint16_t rocket_num = (uint16_t)(long int)arg;

  if ( rocket_num <= RLTOP )
  {
    // Turn master power off
    //gpio_set_level (ROCKET_POWER, 0);
    // Reset rocket relay
    gpio_set_level ((gpio_num_t)rockets[rocket_num].gpioPin, ROCKET_SAFE);
  }

  // Tidy up...
  esp_timer_delete (reset_task_handle);
  reset_task_handle = NULL;
}

// Launch the rocket and set the power off timer
int _launch_rocket (char *buf, int bufsize, char *param, char *value, int type)
{
  uint16_t rocket_num = atoi (value);

  if ( rocket_num <= RLTOP )
  {
    if ( reset_task_handle == NULL )
    {
      // Turn the master power on
      //gpio_set_level (ROCKET_POWER, 1);
      // Launch the rocket
      gpio_set_level ((gpio_num_t)rockets[rocket_num].gpioPin, ROCKET_LAUNCH);

      // Prepare the reset
      const esp_timer_create_args_t rocket_reset_args = {
        .callback = &rocket_reset_callback,
        .arg = (void *)(long int)rocket_num,
        .name = "rocket reset"};
      if ( esp_timer_create (&rocket_reset_args, &reset_task_handle) == ESP_OK )
      {
        esp_timer_start_once (reset_task_handle, 3000000);
      }
    }
  }

  return true;
}
#endif // CONFIG_AEOLIAN_DEV_ROCKET
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
enum schedule sched2int (char *schedule)
{
  enum schedule sched;

  if ( !str_cmp ("on", schedule) )
  {
    sched = SCHED_ON;    // On
  }
  else if ( !str_cmp ("off", schedule) )
  {
    sched = SCHED_OFF;    // Off
  }
  else
  {
    sched = SCHED_AUTO;    // Auto (default)
  }

  return sched;
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
const char *int2sched (enum schedule sched)
{
  switch ( sched )
  {
    case SCHED_ON:
      return "on";
      break;
    case SCHED_OFF:
      return "off";
      break;
    default:
      return "auto";
  }
}
int _set_ledSwitch (char *buf, int bufsize, char *param, char *value, int type)
{
  uint8_t ns = atoi(value);
  F_LOGV(true, true, LC_MAGENTA, "_set_ledSwitch: param = %s, ns = %d", param, ns);

  if ( ns < SCHED_MAX )
  {
    control_vars.schedule = ns;
    save_nvs_num(NVS_LIGHT_CONFIG, "schedule", value, (tNumber)type);
  }

  // Apply the config quickly
  if ( control_vars.schedule == SCHED_OFF )
  {
    lightsPause(PAUSE_USER_REQ);
  }
  else
  {
    lightsUnpause(PAUSE_USER_REQ, control_vars.schedule == SCHED_ON?false:true);
  }

  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, control_vars.schedule));
}

// ***********************************************
int _get_apAddress (char *buf, int bufsize, int unused)
{
  uint16_t len = 0;

  // Get our WiFi mode
  wifi_mode_t mode;
  esp_wifi_get_mode (&mode);

  if ( mode & WIFI_MODE_AP )
  {
    //FIXMEbuflen = snprintf(buf, bufsize, IPSTR, IP2STR(&ipconfig.ip.addr));
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  return len;
}
// ***********************************************
int _get_bootCount (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%lu (last ota @ %lu)", boot_count, ota_update);
}
// ***********************************************
int _get_buildTarget (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%s", CONFIG_TARGET_DEVICE_STR);
}
// ***********************************************
int _get_buildVersion (char *buf, int bufsize, int param)
{
  // Something broke, so using an alternative method
  //return snprintf (buf, bufsize, "%s", esp_get_idf_version ());
  return snprintf(buf, bufsize, "%s", AEOLIAN_FW_HASH);
}
// ***********************************************
int _get_log_history (char *buf, int bufsize, int type)
{
  return snprintf(buf, bufsize, "%s", CONFIG_TARGET_DEVICE_STR);
}
// ***********************************************
int _get_curPattern (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%s", patterns[control_vars.cur_pattern].name);
}
int _set_curPattern (char *buf, int bufsize, char *param, char *value, int dmmy)
{
  uint16_t pNum = atoi (value);
  if ( pNum <= control_vars.num_patterns && patterns[pNum].enabled )
  {
    set_pattern (pNum);
  }
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, control_vars.cur_pattern));
}
// ***********************************************
int _get_curTheme (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%s", patterns[control_vars.cur_pattern].name);
}
int _set_curTheme (char *buf, int bufsize, char *param, char *value, int dmmy)
{
  if ( set_theme(atoi(value)) == ESP_OK )
  {
    BSET(control_vars.bitflags, DISP_BF_MANUAL_THEME);
  }
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, control_vars.cur_themeID));
}
// ***********************************************
int _get_date (char *buf, int bufsize, int type)
{
  time_t now;
  time (&now);

  struct tm *dt;
  dt = localtime (&now);

  return snprintf (buf, bufsize, "%d-%02d-%02d", dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday);
}
// ***********************************************
int _get_dateTime (char *buf, int bufsize, int dmmy)
{
  time_t now;
  time (&now);

  struct tm *dt;
  dt = localtime (&now);

  return snprintf (buf, bufsize, "%.19s", asctime (dt));
}
// ***********************************************
int _get_heap (char *buf, int bufsize, int dmmy)
{
  return snprintf (buf, bufsize, "%lu", esp_get_free_heap_size ());
}
// ***********************************************
int _set_hostname (char *buf, int bufsize, char *param, char *value, int zone)
{
  uint16_t len = 0;
  return len;
}
int _get_hostname (char *buf, int bufsize, int dmmy)
{
  const char *hostName;
  short len = 0;

  if ( wifi_get_hostname (&hostName) == ESP_OK )
  {
    len = snprintf (buf, bufsize, "%s", hostName);
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  // Return character count
  return len;
}
// ***********************************************
int _get_ipAddress (char *buf, int bufsize, int dmmy)
{
  uint16_t len = 0;

  // Get our WiFi mode
  wifi_mode_t mode;
  esp_wifi_get_mode (&mode);

  if ( mode & WIFI_MODE_STA )
  {
    len = wifi_get_IpStr (buf, bufsize);
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  return len;
}
// ***********************************************
int _get_led_count (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", control_vars.pixel_count);
}
// ***********************************************
int _get_led_dim (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", control_vars.dim);
}
int _set_led_dim (char *buf, int bufsize, char *param, char *value, int type)
{
  set_brightness(atoi(value));
  return(snprintf(buf, bufsize, JSON_RESPONSE_AS_STRVAL, param, value));
}
// ***********************************************
int _get_gpio_led_pin (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", control_vars.led_gpio_pin);
}
// ***********************************************
int _get_gpio_light_pin (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", control_vars.light_gpio_pin);
}
// ***********************************************
int _get_ledSwitch (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", control_vars.schedule);
}
// ***********************************************
int _get_lightSwitch (char *buf, int bufsize, int type)
{
  return snprintf (buf, bufsize, "%d", lightStatus (SECURITY_LIGHT));
}
int _set_lightSwitch (char *buf, int bufsize, char *param, char *value, int type)
{
  set_light (SECURITY_LIGHT, atoi (value), 50);
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_STRVAL, param, lightStatus (SECURITY_LIGHT)?"1":"0"));
}
// ***********************************************
int _get_macAddress (char *buf, int bufsize, int type)
{
  uint16_t len = 0;

  // Get our WiFi mode
  wifi_mode_t mode;
  esp_wifi_get_mode (&mode);

  if ( mode & WIFI_MODE_STA )
  {
    uint8_t macaddr[6] = {};
    esp_efuse_mac_get_default(macaddr);
    len = snprintf (buf, bufsize, MACSTR, MAC2STR (macaddr));
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  return len;
}
// ***********************************************
int _get_macAddressAp (char *buf, int bufsize, int param)
{
  uint16_t len = 0;

  // Get our WiFi mode
  wifi_mode_t mode;
  esp_wifi_get_mode (&mode);

  wifi_config_t config = {};

  if ( mode & WIFI_MODE_AP )
  {
    len = snprintf (buf, bufsize, MACSTR, MAC2STR (config.sta.bssid));
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  return len;
}
// ***********************************************
int _get_zone_start (char *buf, int bufsize, int zone)
{
  return snprintf (buf, bufsize, "%d", overlay[zone].zone_params.start);
}
// ***********************************************
int _get_zone_count (char *buf, int bufsize, int zone)
{
  return snprintf (buf, bufsize, "%d", overlay[zone].zone_params.count);
}
// ***********************************************
esp_err_t _set_zone (uint16_t zone)
{
  nvs_handle_t nvs_handle;
  char zoneStr[64];
  sprintf(zoneStr, "zone_%2d", zone);

  esp_err_t err = nvs_open(NVS_LIGHT_CONFIG, NVS_READWRITE, &nvs_handle);
  if ( err == ESP_OK )
  {
    err = nvs_set_blob(nvs_handle, zoneStr, &overlay[zone], sizeof (overlay_t));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
  }
  return err;
}
int _set_zone_start (char *buf, int bufsize, char *param, char *value, int zone)
{
  overlay[zone].zone_params.start = atoi (value);
  _set_zone (zone);
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, overlay[zone].zone_params.start));
}
// ***********************************************
int _set_zone_count (char *buf, int bufsize, char *param, char *value, int zone)
{
  overlay[zone].zone_params.count = atoi (value);
  _set_zone (zone);
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, overlay[zone].zone_params.count));
}
// ***********************************************
int _get_sunrise (char *buf, int bufsize, int param)
{
  return snprintf (buf, bufsize, "%.8s", (char *)&(ctime (&lightcheck.ts_sunrise))[11]);
}
// ***********************************************
int _get_sunset (char *buf, int bufsize, int param)
{
  return snprintf (buf, bufsize, "%.8s", (char *)&(ctime (&lightcheck.ts_sunset))[11]);
}
// ***********************************************
int _get_sysRstInfo (char *buf, int bufsize, int param)
{
  strcpy (buf, verbose_print_reset_reason (rtc_get_reset_reason (0)));
  return strlen (buf);
}
// ***********************************************
int _get_time (char *buf, int bufsize, int param)
{
  time_t now;
  time (&now);

  struct tm *dt;
  dt = localtime (&now);

  return snprintf (buf, bufsize, "%02d:%02d:%02d", dt->tm_hour, dt->tm_min, dt->tm_sec);
}
// ***********************************************
int _get_moonphase (char *buf, int bufsize, int param)
{
  phaseinfo_t moonPhase;
  get_moonphase (&moonPhase);

  return snprintf (buf, bufsize, "Age: %2.2f days, Phase: %s", moonPhase.age, moonPhase.nameStr);
}
// ***********************************************
int _get_uptime (char *buf, int bufsize, int param)
{
  // create tm struct
  time_t now;
  time (&now);

  uint64_t uptime = esp_timer_get_time () / 1000000;
  uint16_t dys = uptime / 86400;
  uint16_t hrs = (uptime % 86400) / 3600;
  uint16_t mns = (uptime % 3600) / 60;
  uint16_t scs = (uptime % 3600) % 60;

  return snprintf (buf, bufsize, "%03d:%02d:%02d:%02d", dys, hrs, mns, scs);
}
// ***********************************************
int _get_memory (char *buf, int bufsize, int param)
{
  return snprintf (buf, bufsize, "%0d", xPortGetFreeHeapSize ());
}
// ***********************************************
int _get_idf_version (char *buf, int bufsize, int param)
{
  // Something broke, so using an alternative method
  //return snprintf (buf, bufsize, "%s", esp_get_idf_version ());
  return snprintf(buf, bufsize, "v%d.%d.%d", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
}
// ***********************************************
static uint8_t _parse_u8 (char *parseStr)
{
  uint8_t tmpInt = 0;
  while ( *parseStr >= '0' && *parseStr <= '9' )
  {
    tmpInt *= 10;
    tmpInt += (int)(*parseStr - '0');
    parseStr++;
  }
  return tmpInt;
}
// ***********************************************
int _set_pattern (char *buf, int bufsize, char *param, char *value, int type)
{
  esp_err_t err = ESP_OK;
  uint8_t pattern = 0;
  uint8_t enabled = 0;
  uint8_t stars = 0;
  uint8_t setEn = false;
  uint8_t setSt = false;

  char *parseStr = value;
  while ( *parseStr && err == ESP_OK )
  {
    switch ( *parseStr++ )
    {
      case 'p':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            pattern *= 10;
            pattern += (int)(*parseStr - '0');
            parseStr++;
          }
          break;
        }
      case 'e':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            enabled *= 10;
            enabled += (int)(*parseStr - '0');
            parseStr++;

            // Flag we are updating enabled status
            setEn = true;
          }
          break;
        }
      case 's':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            stars *= 10;
            stars += (int)(*parseStr - '0');
            parseStr++;

            // Flag we are updating the number of stars
            setSt = true;
          }
          break;
        }
      default:
        err = ESP_FAIL;
        break;
    }
  }

  if ( err == ESP_OK )
  {
    if ( pattern <= control_vars.num_patterns )
    {
      if ( setEn )
      {
        patterns[pattern].enabled = enabled > 0?1:0;
      }

      if ( setSt )
      {
        patterns[pattern].num_stars = stars;
      }

      // TODO: Save to NVS
    }
  }

  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}
// ***********************************************
int _set_palette (char *buf, int bufsize, char *param, char *value, int type)
{
  esp_err_t err = ESP_OK;
  uint8_t theme = 0;
  uint8_t palette = 0;
  uint8_t colour = 0;
  uint32_t hex = 0;

  F_LOGV(true, true, LC_GREY, "param: %s, value: %s", param, value);
  char *parseStr = value;
  while ( *parseStr && err == ESP_OK )
  {
    switch ( *parseStr++ )
    {
      case 't':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            theme *= 10;
            theme += (int)(*parseStr - '0');
            parseStr++;
          }
          break;
        }
      case 'p':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            palette *= 10;
            palette += (int)(*parseStr - '0');
            parseStr++;
          }
          break;
        }
      case 'c':
        {
          while ( *parseStr >= '0' && *parseStr <= '9' )
          {
            colour *= 10;
            colour += (int)(*parseStr - '0');
            parseStr++;
          }
          break;
        }
      case ':':
        {
          int x = 0;
          for ( int i = 0; i < 6; i++ )
          {
            hex = hex << 4;
            x = (*parseStr++ - '0') & ~ 32;
            if ( x <= 9 )
            {
              hex += x;
            }
            else
            {
              hex += (x - 7);
            }
          }
          break;
        }
        break;
      default:
        err = ESP_FAIL;
        break;
    }
  }

  if ( err == ESP_OK )
  {
    if ( theme < control_vars.num_themes )
    {
      if ( palette < themes[theme].count )
      {
        if ( colour < 16 )
        {
          // Set the colour
          themes[theme].list[palette].entries[colour].r = (hex >> 16) & 0xFF;
          themes[theme].list[palette].entries[colour].g = (hex >> 8) & 0xFF;
          themes[theme].list[palette].entries[colour].b = (hex & 0xFF);

          // Show a preview
          showColourPreview (themes[theme].list[palette].entries[colour]);
        }
      }
    }
  }

  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}

// Bootloader
#if defined (CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_PERF)
#define BOOT_OPT    1
#elif defined CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_SIZE
#define BOOT_OPT    2
#else
#define BOOT_OPT    0
#endif

// Application
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
#define APP_OPT     1
#elif defined CONFIG_COMPILER_OPTIMIZATION_SIZE
#define APP_OPT     2
#else
#define APP_OPT     0
#endif

// Heap debug
#if defined (CONFIG_HEAP_POISONING_DISABLED)
#define HEAP_P      0
#else
#define HEAP_P      1
#endif

// Heap Tracing
#if defined (CONFIG_HEAP_TRACING_OFF)
#define HEAP_T      0
#else
#define HEAP_T      1
#endif

// ***********************************************
int _get_buildOpts (char *buf, int bufsize, int param)
{
  int len = 0;

  // These settings are for release
  if ( APP_OPT == 1 && BOOT_OPT == 1 && !(HEAP_P + HEAP_T) )
  {
    len = snprintf (buf, bufsize, "Release");
  }
  else if ( HEAP_P == 1 )
  {
    len = snprintf (buf, bufsize, "Heap trace/poison");
  }
  else
  {
    len = snprintf (buf, bufsize, "Debug");
  }

  return len;
}
// ***********************************************
int _get_versionDate (char *buf, int bufsize, int unused)
{
  return snprintf (buf, bufsize, __DATE__ " " __TIME__);
}
int _get_ap_ssid (char *buf, int bufsize, int unused)
{
  uint16_t len = 0;

  // Get our WiFi mode
  wifi_mode_t mode;
  esp_wifi_get_mode (&mode);

  wifi_config_t config;

  if ( mode & WIFI_MODE_STA )
  {
    if ( ESP_OK == esp_wifi_get_config (WIFI_IF_STA, &config) )
    {
      len = snprintf (buf, bufsize, "%s", config.sta.ssid);
    }
  }
  else
  {
    len = snprintf (buf, bufsize, "%s", "--");
  }

  return len;
}

// ******************************************************************************************
// Note: Sorted into alphabetical order at runtime.
// ******************************************************************************************
typedef struct
{
  char  name[16];
  int  (*get_param)(char *, int, int);
  int  (*set_param)(char *, int, char *, char *, int);
  int  arg;
} status_command_t;

WORD_ALIGNED_ATTR DRAM_ATTR static status_command_t status_command[] = {
  {STR_AP_IP_ADDR,              _get_ap_setting,       _set_ap_setting,        AP_IP_ADDR},
  {STR_AP_AUTHMODE,             _get_ap_setting,       _set_ap_setting,        AP_AUTHMODE},
  {STR_AP_BANDWIDTH,            _get_ap_setting,       _set_ap_setting,        AP_BANDWIDTH},
  {STR_AP_PRIMARY,              _get_ap_setting,       _set_ap_setting,        AP_PRIMARY},
  {STR_AP_CYPHER,               _get_ap_setting,       _set_ap_setting,        AP_CYPHER},
  {STR_AP_HIDDEN,               _get_ap_setting,       _set_ap_setting,        AP_HIDDEN},
  {STR_AP_PASSW,                _get_ap_setting,       _set_ap_setting,        AP_PASSWORD},
  {STR_AP_SSID,                 _get_ap_setting,       _set_ap_setting,        AP_SSID},
  {"boot_count",                _get_bootCount,        NULL,                   0},
  {"build_options",             _get_buildOpts,        NULL,                   0},
  {"build_target",              _get_buildTarget,      NULL,                   0},
  {"build_version",             _get_buildVersion,     NULL,                   0},
  {"cur_pattern",               _get_curPattern,       NULL,                   0},
  {"date",                      _get_date,             NULL,                   0},
  {"date_time",                 _get_dateTime,         NULL,                   0},
  {"dim",                       _get_led_dim,          _set_led_dim,           0},
  {"heap",                      _get_heap,             NULL,                   0},
  {"hostname",                  _get_hostname,         _set_hostname,          0},
  {"idf_version",               _get_idf_version,      NULL,                   0},
  {"info_led",                  NULL,                  NULL,                   0},
  {"ip_address",                _get_ipAddress,        NULL,                   0},
#if defined (CONFIG_AEOLIAN_DEV_ROCKET)
  {"launch_rocket",             NULL,                  _launch_rocket,         TYPE_U8},
#endif
  {"led_count",                 _get_led_count,        _set_led_control,       TYPE_U16},
  {"led_gpio_pin",              _get_gpio_led_pin,     _set_led_control,       TYPE_U8},
  {"led_switch",                _get_ledSwitch,        _set_ledSwitch,         TYPE_U8},
  {"light_gpio_pin",            _get_gpio_light_pin,   _set_led_control,       TYPE_U8},
  {"light_switch",              _get_lightSwitch,      _set_lightSwitch,       0},
  {"log_history",               _get_log_history,      NULL,                   0},
  {"mac_address",               _get_macAddress,       NULL,                   0},
  {"mac_address_ap",            _get_macAddressAp,     NULL,                   0},
  {"memory",                    _get_memory,           NULL,                   0},
  {"moonphase",                 _get_moonphase,        NULL,                   0},
  {STR_MQTT_CLIENT_ID,          _get_mqtt_setting,     _set_mqtt_setting,      MQTT_CLIENT_ID},
  {STR_MQTT_CLIENT_KEEPALIVE,   _get_mqtt_setting,     _set_mqtt_setting,      MQTT_KEEPALIVE},
  {STR_MQTT_CLIENT_MASTER,      _get_mqtt_setting,     _set_mqtt_setting,      MQTT_MASTER},
  {STR_MQTT_CLIENT_PASSWORD,    _get_mqtt_setting,     _set_mqtt_setting,      MQTT_PASSWORD},
  {STR_MQTT_CLIENT_PORT,        _get_mqtt_setting,     _set_mqtt_setting,      MQTT_PORT},
  {STR_MQTT_CLIENT_SLAVE,       _get_mqtt_setting,     _set_mqtt_setting,      MQTT_SLAVE},
  {STR_MQTT_CLIENT_URI,         _get_mqtt_setting,     _set_mqtt_setting,      MQTT_URI},
  {STR_MQTT_CLIENT_USERNAME,    _get_mqtt_setting,     _set_mqtt_setting,      MQTT_USERNAME},
  {"pattern",                   NULL,                  _set_curPattern,        0},
  {"set_palette",               NULL,                  _set_palette,           0},
  {"set_pattern",               NULL,                  _set_pattern,           0},
  {STR_STA_PASSW,               _get_sta_setting,      _set_sta_setting,       STA_PASSWORD},
  {STR_STA_UNAME,               _get_sta_setting,      _set_sta_setting,       STA_USERNAME},
  {STR_STA_SSID,                _get_sta_setting,      _set_sta_setting,       STA_SSID},
  {"sunrise",                   _get_sunrise,          NULL,                   0},
  {"sunset",                    _get_sunset,           NULL,                   0},
  {"sys_led",                   NULL,                  NULL,                   0},
  {"sys_rst_info",              _get_sysRstInfo,       NULL,                   0},
  {"theme",                     NULL,                 _set_curTheme,           0},
  {"time",                      _get_time,             NULL,                   0},
  {"uptime",                    _get_uptime,           NULL,                   0},
  {"version_date",              _get_versionDate,      NULL,                   0},
  {"wifi_mode",                 _get_wifi_setting,     _set_wifi_setting,      WIFI_MODE},
  {"wifi_powersave",            _get_wifi_setting,     _set_wifi_setting,      WIFI_POWERSAVE},
  {"z0c",                       _get_zone_count,       _set_zone_count,        0},
  {"z0s",                       _get_zone_start,       _set_zone_start,        0},
  {"z1c",                       _get_zone_count,       _set_zone_count,        1},
  {"z1s",                       _get_zone_start,       _set_zone_start,        1},
  {"z2c",                       _get_zone_count,       _set_zone_count,        2},
  {"z2s",                       _get_zone_start,       _set_zone_start,        2},
  {"z3c",                       _get_zone_count,       _set_zone_count,        3},
  {"z3s",                       _get_zone_start,       _set_zone_start,        3},
};
#define DSCTOP ((sizeof(status_command) / sizeof(status_command_t)) -1)
// --------------------------------------------------------------------------
// Ensure the above commands are in alphabetical order
// --------------------------------------------------------------------------
static int sCommandSortFunc(const void *a, const void *b)
{
  return shrt_cmp (((const status_command_t *)a)->name, ((const status_command_t *)b)->name);
}
void init_device_commands (void)
{
  qsort (status_command, DSCTOP, sizeof (status_command_t), sCommandSortFunc);
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
IRAM_ATTR bool set_implemented (int i)
{
  return status_command[i].set_param != NULL;
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR int get_command(char *arg)
#else
int get_command (char *arg)
#endif
{
  int top = DSCTOP;
  int mid, found, low = 0;

  while ( low <= top )
  {
    mid = (low + top) / 2;

    if ( (found = str_cmp (arg, status_command[mid].name)) > 0 )
      low = mid + 1;
    else if ( found < 0 )
      top = mid - 1;
    else
      return(mid);
  }

  return(-1);
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t getDeviceSetting(char *buf, int *buflen, char *token)
#else
esp_err_t getDeviceSetting(char *buf, int *buflen, char *token)
#endif
{
  esp_err_t err = ESP_FAIL;
  int i;

  if ( token != NULL )
  {
    if ( (i = get_command(token)) > 0 )
    {
      // Valid: We handle this token, even if it may not be set yet.
      err = ESP_OK;

      *buflen = ((*status_command[i].get_param)(buf, *buflen, status_command[i].arg));
      //printf("getDeviceSetting: %.*s (%d)\n", *buflen, buf, *buflen);
    }
  }

  return err;
}

// Template code for the "Device Status" page.
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR esp_err_t tplDeviceConfig(struct async_resp_arg *resp_arg, char *token)
#else
IRAM_ATTR esp_err_t tplDeviceConfig(httpd_req_t *req, char *token)
#endif
{
  esp_err_t err = ESP_FAIL;
  char buf[BUF_SIZE + 1];
  int buflen = 0;
  int i;

  if ( token != NULL )
  {
    if ( (i = get_command(token)) < 0 )
    {
      buflen = snprintf (buf, BUF_SIZE, "Unknown Param: %%%s%%", token);
    }
    else
    {
      buflen = ((*status_command[i].get_param)(buf, BUF_SIZE, status_command[i].arg));
      if ( buflen > 0 )
      {
#if defined (CONFIG_HTTPD_USE_ASYNC)
        httpd_socket_send(resp_arg->hd, resp_arg->fd, buf, buflen, 0);
#else
        httpd_resp_send_chunk(req, buf, buflen);
#endif
      }
      err = ESP_OK;
    }
  }

  return err;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t tplSetConfig(char *json_resp_buf, int resp_buf_size, char *param, char *value)
#else
esp_err_t tplSetConfig(char *json_resp_buf, int resp_buf_size, char *param, char *value)
#endif
{
  esp_err_t err = ESP_FAIL;
  int i;

  if ( param != NULL )
  {
    if ( (i = get_command(param)) >= 0 )
    {
      err = ((*status_command[i].set_param)(json_resp_buf, resp_buf_size, param, value, status_command[i].arg));
    }
  }

  return err;
}

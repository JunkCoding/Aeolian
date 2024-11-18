


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include <lwip/ip_addr.h>

#include <esp_task_wdt.h>
#include <esp_types.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include <nvs_flash.h>

#include <mqtt_config.h>
#include <mqtt_client.h>

#include <jsmn.h>

#include "app_main.h"
#include "app_utils.h"
#include "app_lightcontrol.h"
#include "app_configs.h"
#include "app_httpd.h"
#include "app_mqtt.h"
#include "app_wifi.h"
#include "app_cctv.h"

const char *STR_DEVID       = "devID";
const char *STR_EVENTTYPE   = "EventType";
const char *STR_CHANNEL     = "Channel";
const char *STR_IVMSCHANNEL = "IvmsChannel";
const char *STR_RULEID      = "RuleId";

#define   MQTT_EVT_BUFSIZE    128
#define   MQTT_BUFSIZE        512

QueueHandle_t mqtt_send_queue = NULL;
EventGroupHandle_t mqtt_event_group = NULL;

const int MQTT_CONNECTED = BIT0;

static volatile esp_mqtt_client_handle_t mqtt_client = NULL;

typedef struct _hikalert_
{
  int                      devId;
  VCA_RULE_EVENT_TYPE_EX   eventType;
  int                      channel;
  int                      ivmsChannel;
  int                      ruleID;
  uint16_t                 sec_light;
  overlay_zone             zone;
  overlay_onmask           mask;
  uint16_t                 time;
  cRGB                     color;
} hik_alert_t;

typedef struct
{
  char     *topic;
  char     *data;
  int       len;
  int       qos;
  int       retain;
} mqtt_mesg_t;

void proc_mqtt_data (esp_mqtt_event_handle_t event);
void mqtt_publish (char *topic, const char *data, int len, int qos, int retain);
int  mqtt_subscribe (char *topic, int qos);
void stop_mqtt_client(esp_mqtt_client_handle_t MQTTClient);

bool on_off (int len, char *data)
{
  bool value = 0;

  // Accept "1"/"0" or "On"/"Off"
  if ( len == 1 )
  {
    value = data[0] == '0'?0:1;
  }
  else
  {
    if ( strncmp ("on", data, len) == 0 )
    {
      value = 1;
    }
    else if ( strncmp ("off", data, len) == 0 )
    {
      value = 0;
    }
  }

  return value;
}
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t mqtt_event_handler (esp_mqtt_event_handle_t event)
#else
esp_err_t mqtt_event_handler (esp_mqtt_event_handle_t event)
#endif
{
  char evtBuf[MQTT_EVT_BUFSIZE + 1] = {};

  switch ( event->event_id )
  {
    case MQTT_EVENT_BEFORE_CONNECT:
      {
        F_LOGE(true, true, LC_RED, "MQTT_EVENT_BEFORE_CONNECT");
        break;
      }
    case MQTT_EVENT_CONNECTED:
      {

        F_LOGI(true, true, LC_GREEN, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED);

        // Get our WiFi mode
        wifi_mode_t mode;
        esp_wifi_get_mode (&mode);

        if ( mode & WIFI_MODE_STA )
        {
          char ip[64];
          wifi_get_IpStr (ip, sizeof (ip));
          snprintf (evtBuf, MQTT_EVT_BUFSIZE, "Connected (%s)", ip);
        }
        else
        {
          snprintf (evtBuf, MQTT_EVT_BUFSIZE, "Connected");
        }

        // Announce ourselves on the network
        mqttPublish (dev_Connect, evtBuf);

        // Listen for other clients on the network
        mqttSubscribe (dev_Connect);

        // Subscribe to our control channels
        mqttSubscribe(dev_Light);
        mqttSubscribe(dev_LEDs);
        mqttSubscribe(dev_Pattern);
        mqttSubscribe(dev_hikAlarm);
        // These are for internal use
#if defined (CONFIG_AXRGB_DEV_OLIMEX) || defined (CONFIG_AXRGB_DEV_TTGO) || defined (CONFIG_AXRGB_DEV_SEGGER) || defined (CONFIG_AXRGB_DEV_DEBUG)
        mqttSubscribe(dev_Network);
        mqttSubscribe(dev_Radar);
#endif
        mqttSubscribe (dev_Lux);
        break;
      }
    case MQTT_EVENT_DISCONNECTED:
      {
        F_LOGW(true, true, LC_RED, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED);
        break;
      }
    case MQTT_EVENT_SUBSCRIBED:
      {
        F_LOGV(true, true, LC_WHITE, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //F_LOGW(true, true, LC_WHITE, "MQTT Subscribed to '%.*s'", event->data_len, event->data);
        break;
      }
    case MQTT_EVENT_UNSUBSCRIBED:
      {
        F_LOGV(true, true, LC_YELLOW, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
      }
    case MQTT_EVENT_PUBLISHED:
      {
        F_LOGV(true, true, LC_GREEN, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
      }
    case MQTT_EVENT_DATA:
      {
        F_LOGV(true, true, LC_GREEN, "MQTT_EVENT_DATA");
        F_LOGV(true, true, LC_GREEN, "TOPIC = %.*s", event->topic_len, event->topic);
        F_LOGV(true, true, LC_GREEN, "DATA = %.*s", event->data_len, event->data);

        proc_mqtt_data(event);
        break;
      }
    case MQTT_EVENT_ERROR:
      {
        esp_mqtt_error_codes_t *error_handle = event->error_handle;
        switch ( error_handle->error_type )
        {
          case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            F_LOGE(true, true, LC_RED, "Transport error");
            break;
          case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            switch ( error_handle->connect_return_code )
            {
              case MQTT_CONNECTION_REFUSE_PROTOCOL: /*!< MQTT connection refused reason: Wrong protocol */
                F_LOGE(true, true, LC_RED, "Connection refused: Wrong protocol");
                break;
              case MQTT_CONNECTION_REFUSE_ID_REJECTED: /*!< MQTT connection refused reason: ID rejected */
                F_LOGE(true, true, LC_RED, "Connection refused: ID rejected");
                break;
              case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE: /*!< MQTT connection refused reason: Server
                                                                 unavailable */
                F_LOGE(true, true, LC_RED, "Connection refused: Server unavailable");
                break;
              case MQTT_CONNECTION_REFUSE_BAD_USERNAME: /*!< MQTT connection refused reason: Wrong user */
                F_LOGE(true, true, LC_RED, "Connection refused: Wrong user");
                break;
              case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED: /*!< MQTT connection refused reason: Wrong username or password */
                F_LOGE(true, true, LC_RED, "Connection refused: Authentication error");
                break;
              default:;
            }
            break;
          case MQTT_ERROR_TYPE_NONE:
          default:;
        }
        break;
      }
    case MQTT_EVENT_DELETED:
      {
        F_LOGE(true, true, LC_RED, "MQTT_EVENT_DELETED");
        break;
      }
    default:
      {
        F_LOGE(true, true, LC_YELLOW, "Other event id:%d", event->event_id);
        break;
      }
  }

  return ESP_OK;
}

void proc_command(uint16_t cmd, char *param, uint16_t reason)
{
  //F_LOGI(true, true, LC_BRIGHT_RED, "proc_command: %d, %s, %d", cmd, param, reason);

  if ( param != NULL )
  {
    uint32_t x;

    switch ( cmd )
    {
      case CMD_LED_PWR:
        {
          if ( on_off(strlen(param), param) )
          {
            lightsUnpause(reason, false);
          }
          else
          {
            lightsPause(reason);
          }
          break;
        }
      case CMD_LED_COL1:
        x = hex2int (param);
        control_vars.this_col.r = (x >> 16) & 0xFF;
        control_vars.this_col.g = (x >> 8) & 0xFF;
        control_vars.this_col.b = (x & 0xFF);
        //hexDump ("COL1", &control_vars.this_col, 3, 16);
        break;
      case CMD_LED_COL2:
        x = hex2int (param);
        control_vars.that_col.r = (x >> 16) & 0xFF;
        control_vars.that_col.g = (x >> 8) & 0xFF;
        control_vars.that_col.b = (x & 0xFF);
        //hexDump ("COL2", &control_vars.that_col, 3, 16);
        break;
      case CMD_LED_PAL1:
        control_vars.thisPalette = atoi(param);
        //hexDump ("PAL1", &control_vars.thisPalette, 1, 16);
        break;
      case CMD_LED_PAL2:
        control_vars.thatPalette = atoi(param);
        //hexDump ("PAL2", &control_vars.thatPalette, 1, 16);
        break;
      case CMD_LED_DIR1:
        if ( atoi(param) > 0 )
        {
          BSET(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
        }
        else
        {
          BCLR(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
        }
        break;
      case CMD_LED_IDX1:
        control_vars.palIndex = atoi(param);
        //hexDump ("IDX1", &control_vars.palIndex, 1, 16);
        break;
      case CMD_LED_SCOL:
        x = hex2int (param);
        control_vars.static_col.r = (x >> 16) & 0xFF;
        control_vars.static_col.g = (x >> 8) & 0xFF;
        control_vars.static_col.b = (x & 0xFF);
        //hexDump ("SCOL", &control_vars.static_col, 3, 16);
        break;
      case CMD_LED_LOOP_US:
        set_loop_delay(atoi(param));
        break;
      case CMD_LED_BRIGHTNESS:
        set_brightness(atoi(param));
        break;
    }
  }
}

void proc_ev_dev_leds (esp_mqtt_event_handle_t event)
{
  /* Process JSON Data */
  char token[64]  __attribute__ ((aligned(4))) = {};
  char param[64]  __attribute__ ((aligned(4))) = {};

  char *endptr;
  jsmn_parser p;
  jsmntok_t t[30];

  jsmn_init (&p);
  int r = jsmn_parse(&p, event->data, event->data_len, t, sizeof(t) / sizeof(t[0]));
  // ToDo: Add to all jsmn_parse()
  if ( r < 0 )
  {
    F_LOGE(true, true, LC_RED, "jsmn_parse error (%d), mqtt data: %.*s", r, event->data_len, event->data);
    return;
  }

  //F_LOGW(true, true, LC_YELLOW, "r: %d, mqtt data: %.*s", r, event->data_len, event->data);

  uint16_t i = 1;
  while ( i < r )
  {
    snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);

#if defined (CONFIG_DEBUG)
    F_LOGV(true, true, LC_BRIGHT_YELLOW, "i: %d, type: %d, start: %d, len = %d, token: %s", i, t[i].type, t[i].start, t[i].end - t[i].start, token);
#endif

    // Are we dealing with a string?
    switch (t[i++].type)
    {
      case JSMN_STRING:
      {
        if ( !shrt_cmp(JSON_STR_CMD, token) )
        {
#if defined (CONFIG_DEBUG)
          F_LOGV(true, true, LC_YELLOW, "type: %d, JSMN_STRING: %.*s", t[i].type, t[i].end - t[i].start, event->data + t[i].start);
#endif
          // Ensure this is an object
          if ( t[i].type == JSMN_OBJECT )
          {
            uint16_t cmd = 0xFF;
            uint16_t reason = PAUSE_NOTPROVIDED;

            // Number of objects
            uint16_t objs = t[i++].size;
            for ( uint16_t v = 0; v < objs; v++, i++ )
            {
              snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
              //F_LOGW(true, true, LC_YELLOW, "Loop: %s", token);
              if ( !shrt_cmp(JSON_STR_ID, token) )
              {
                i++;
                cmd = (uint16_t)strtol(event->data + t[i].start, &endptr, 10);
              }
              else if ( !shrt_cmp(JSON_STR_ARG, token) )
              {
                i++;
                sprintf(param, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
              }
              else if ( !shrt_cmp(JSON_STR_REASON, token) )
              {
                i++;
                reason = (uint16_t)strtol(event->data + t[i].start, &endptr, 10);
              }
            }

            if ( cmd != 0xFF )
            {
              proc_command(cmd, param, reason);
            }
          }
        }
        else if ( !shrt_cmp(JSON_STR_SYNC, token) )
        {
#if defined (CONFIG_DEBUG)
          F_LOGV(true, true, LC_GREY, "type: %d, JSMN_STRING: %.*s", t[i].type, t[i].end - t[i].start, event->data + t[i].start);
#endif
          // Save some CPU cycles by Only processing these packets when we are a slave.
          if ( MQTT_Client_Cfg.Slave == true )
          {
            // Ensure this is an object
            if ( t[i].type == JSMN_ARRAY )
            {
              uint16_t paused = 0, pauseReason = 0, themeID = 0;
              uint16_t objs = t[i++].size;
              for ( uint16_t v = 0; v < objs; v++, i++ )
              {
                snprintf(param, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
                switch(v)
                {
                  case 0:
                    if ( shrt_cmp(JSON_STR_SYNC_VER, token) )
                    {
                      control_vars.master_alive = MASTER_TIMEOUT;
                    }
                    else
                    {
                      v = (objs - 1);
                      i = r;
                    }
                    break;
                  case 1:
                    paused = atoi(param);
                    break;
                  case 2:
                    pauseReason = atoi(param);
                    break;
                  case 3:
                    if ( atoi(param) > 0 )
                    {
                      BSET(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
                    }
                    else
                    {
                      BCLR(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
                    }
                    break;
                  case 4:
                    if ( atoi(param) > 0 )
                    {
                      BSET(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT);
                    }
                    else
                    {
                      BCLR(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT);
                    }
                    break;
                  case 5:
                    control_vars.thisPalette = atoi(param);
                    break;
                  case 6:
                    control_vars.thatPalette = atoi(param);
                    break;
                  case 7:
                    themeID = atoi(param);
                    break;
                  case 8:
                    control_vars.cur_pattern = atoi(param);
                    break;
                  case 9:
                    set_brightness(atoi(param));
                    break;
                  case 10:
                    control_vars.delay_us = atoi(param);
                    break;
                  case 11:
                    proc_command(CMD_LED_COL1, param, 0);
                    break;
                  case 12:
                    proc_command(CMD_LED_COL2, param, 0);
                    break;
                  case 13:
                    proc_command(CMD_LED_SCOL, param, 0);
                    break;
                }
              }
  #if defined (CONFIG_DEBUG)
              F_LOGV(true, true, LC_WHITE, "paused: %d (cvars paused: %d), pauseReason: %d", paused, BTST(control_vars.bitflags, DISP_BF_PAUSED), pauseReason);
  #endif
              // Does our state match the master's requested state?
              if ( paused != BTST(control_vars.bitflags, DISP_BF_PAUSED) )
              {
                if ( paused )
                {
                  lightsPause(pauseReason);
                }
                else
                {
                  lightsUnpause(pauseReason, false);
                }
              }

              if ( control_vars.cur_themeID != themeID )
              {
                set_theme(themeID);
              }
            }
          }
        }
        else if ( !shrt_cmp(JSON_STR_THEME, token) )
        {
          snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
          set_theme(atoi(token));
          i++;
        }
        else if ( !shrt_cmp(JSON_STR_PATTERN, token) )
        {
          snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
          set_pattern(atoi(token));
          i++;
        }
        break;
      }
      default:
      {
        //F_LOGW(true, true, LC_YELLOW, "Unhandled: i = %d, type: %d, %s", i, t[i-1].type, token);
        break;
      }
    }
  }
}

#define OV_EXT_LIGHTS   OV_MASK_NIGHT
#define OV_INT_LIGHTS   (overlay_onmask)(OV_MASK_PAUSED | OV_MASK_NIGHT | OV_MASK_DAY)
#define OV_EXT_ROAD     (overlay_onmask)(OV_MASK_PAUSED | OV_MASK_DAY)
  // MASK_PAUSED = 0x01, MASK_RUNNING = 0x02, MASK_DAYTIME = 0x04, MASK_NIGHTTIME = 0x08
  // ZONE_01 = 0x01, ZONE_02 = 0x02, ZONE_03 = 0x04, ZONE_04 = 0x08
DRAM_ATTR hik_alert_t hik_alert[] = {
  //                             <--   Received   -->                    |
  //         |                                      |      | ivms | rule | sec.  | on                                      | on            | dura-  |
  //  devId  | eventType                            | chan | Chan | ID   | light | zones                                   | mask          | tion   | color
#if defined (CONFIG_AXRGB_DEV_WORKSHOP)
  // ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  // 1. Caravan Camera
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (close left)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (the gap)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (wall)
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, intrusion (front of caravan)
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, region entrance (entrance)
  // 2. Rear Camera
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by entrance
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by caravan door
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by car
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_03|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by stables
  // 3. Garden Camera
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, line crossing left of camera
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, line crossing front of camera
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, line crossing by house
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, line crossing on the street
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #1
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #1
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #2
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x20,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #2
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x20,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #3
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0x20,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #3
  // 4. Front Camera.
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  1000,    Maroon},        // Front, line crossing (car uphill travel)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  1000,    HotPink},       // Front, line crossing (car any direction)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  1000,    HotPink},       // Front, line crossing (car any direction)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  1000,    Violet},        // Front, line crossing (car downhill travel)
#elif defined (CONFIG_AXRGB_DEV_CARAVAN)
  // --------------------------------------------------------------------------------------------------------------
  // Caravan Camera
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x05,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (close left)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x05,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (the gap)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x05,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (wall)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x05,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, line crossing (wall)
//{1,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, region entrance (twat)
//{1,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, region entrance (twat)
//{1,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, region entrance (entrance)
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, intrusion detection (close left)
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, intrusion detection (front of)
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Caravan, intrusion detection (entrance)
  // Rear Camera
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by entrance
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by caravan door
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01|OV_ZONE_04),    OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by car
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Rear, line crossing by stables
  // 3. Garden Camera
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #1
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #1
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #2
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #2
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion detection #3
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0x02,   (overlay_zone)(OV_ZONE_NZ),               OV_EXT_LIGHTS,  30000,   MediumWhite},   // Garden, intrusion (continuation) #3
  // 4. Front Camera.
//{4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_EXT_LIGHTS,  1000,    DarkGreen},     // Front, line crossing #1
#else
  // ----------------------------------------------------------------------------------------------------------------------------------------------------------------
  // 1. Caravan Camera
  // ****************************************************************************************************************************************************************
  // Line crossing
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkBlue},      // Caravan, line crossing (left of caravan)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkBlue},      // Caravan, line crossing (left of planter)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkBlue},      // Caravan, line crossing (right of planter)
  {1,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkBlue},      // Caravan, line crossing (entrance)
  // Intrusion detection
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Caravan, left of.
  {1,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Caravan, left of.
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Caravan, in front of.
  {1,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Caravan, in front of.
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    DarkRed},       // Caravan, Twat.
  {1,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    DarkRed},       // Caravan, Twat.
  {1,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    OrangeRed},     // Caravan, lane.
  {1,        ENUM_VCA_EVENT_DURATION,                1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_03),               OV_INT_LIGHTS,  2000,    OrangeRed},     // Caravan, lane.
  // ****************************************************************************************************************************************************************
  // 2. Rear Camera
  // ****************************************************************************************************************************************************************
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkBlue},      // Rear, line crossing by caravan door
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkBlue},      // Rear, line crossing by car
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, line crossing by Beryl & Reese
  {2,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkBlue},      // Rear, line crossing by stables
  {2,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, Enter area by gate
  {2,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion detection (caravan door)
  {2,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion (continuation)
  {2,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion detection (car)
  {2,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion (continuation)
  {2,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    Green},         // Rear, intrusion detection (far right)
  {2,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    Green},         // Rear, intrusion (continuation)
  {2,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion detection (stables)
  {2,        ENUM_VCA_EVENT_DURATION,                1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_04),               OV_INT_LIGHTS,  2000,    DarkOrange},    // Rear, intrusion (continuation)
  // ****************************************************************************************************************************************************************
  // 3. Garden Camera
  // ****************************************************************************************************************************************************************
// Line crossing
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  2000,    Magenta},       // Garden, line crossing by workshop
  {3,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  5000,    Magenta},       // Garden, line crossing by house
// Intrusion detection
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  2000,    DeepPink},      // Garden, intrusion detection #1
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  2000,    DeepPink},      // Garden, intrusion (continuation) #1
  {3,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  2000,    Purple},        // Garden, intrusion detection #2
  {3,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_02),               OV_INT_LIGHTS,  2000,    Purple},        // Garden, intrusion (continuation) #2
  // ****************************************************************************************************************************************************************
  // 4. Front Camera
  // ****************************************************************************************************************************************************************
  // Line crossing
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  200,     Maroon},        // Front, line crossing (car uphill travel)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  200,     HotPink},       // Front, line crossing (car any direction)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  200,     HotPink},       // Front, line crossing (car any direction)
  {4,        ENUM_VCA_EVENT_TRAVERSE_PLANE,          1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  200,     Violet},        // Front, line crossing (car downhill travel)
//  Enter area
  {4,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  300,     HotPink},       // Front, road traffic
// Leave area
//{4,        ENUM_VCA_EVENT_EXIT_AREA,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Front, exit  area in front of garage door
//{4,        ENUM_VCA_EVENT_EXIT_AREA,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    Red},           // Front, exit area by Dildo's driveway
//{4,        ENUM_VCA_EVENT_EXIT_AREA,               1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    Gold},          // Front, exit area in front of Dildo's wall
// Intrusion
  {4,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  1000,    Yellow},        // Front, sidewalk, slow passerby?
  {4,        ENUM_VCA_EVENT_DURATION,                1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  2000,    Yellow},        // Front, sidewalk (continuation)
  {4,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Front, driveway, potential visitor?
  {4,        ENUM_VCA_EVENT_DURATION,                1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Front, driveway,(continuation)
  #if defined(CONFIG_CCTV)
  {4,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0xFF,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    DarkOrange},    // Front, driveway, potential visitor?
  {4,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0xFF,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Front, driveway,(continuation)
  {4,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     0,     0xFF,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    Red},           // Front, enter area in front of garage door
  #else
  {4,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    DarkOrange},    // Front, postbox, possible delivery?
  {4,        ENUM_VCA_EVENT_DURATION,                1,     1,     2,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkOrange},    // Front, postbox, (continuation)
  {4,        ENUM_VCA_EVENT_ENTER_AREA,              1,     1,     0,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    Red},           // Front, enter area in front of garage door
  #endif
  {4,        ENUM_VCA_EVENT_INTRUSION,               1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  5000,    DarkRed},       // Front, front of Dildo's house.
  {4,        ENUM_VCA_EVENT_DURATION,                1,     1,     3,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  3000,    DarkRed},       // Front, front of Dildo's house,(continuation)
// Object placed
//{4,        ENUM_VCA_EVENT_LEFT,                    1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  8000,    BlueViolet},    // Front, object placed
// Object renoved
//{4,        ENUM_VCA_EVENT_TAKE,                    1,     1,     1,     0x00,   (overlay_zone)(OV_ZONE_01),               OV_INT_LIGHTS,  8000,    DarkSlateBlue}, // Front, object removed
#endif
};
#define HKATOP ((sizeof(hik_alert) / sizeof(struct _hikalert_)) -1)

void proc_ev_dev_hikAlarm (esp_mqtt_event_handle_t event)
{
  /* Process JSON Data */
  char token[64] __attribute__ ((aligned(4))) = {};
  int devId = 0, eventType = 0, channel = 0, ivmsChannel = 0, ruleID = 0;
  int i, r, v = 0;
  char *endptr;
  jsmn_parser p;
  jsmntok_t t[12];

  jsmn_init (&p);
  r = jsmn_parse(&p, event->data, event->data_len, t, sizeof (t) / sizeof (t[0]));

  for ( i = 1; i < r; i++ )
  {
    snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);

    if ( !shrt_cmp(STR_DEVID, token) )
    {
      devId = (int)strtol(event->data + t[i + 1].start, &endptr, 10);
      i++; v++;
    }
    else if ( !shrt_cmp(STR_EVENTTYPE, token) )
    {
      eventType = (int)strtol(event->data + t[i + 1].start, &endptr, 10);
      i++; v++;
    }
    else if ( !shrt_cmp(STR_CHANNEL, token) )
    {
      channel = (int)strtol(event->data + t[i + 1].start, &endptr, 10);
      i++; v++;
    }
    else if ( !shrt_cmp (STR_IVMSCHANNEL, token) )
    {
      ivmsChannel = (int)strtol(event->data + t[i + 1].start, &endptr, 10);
      i++; v++;
    }
    else if ( !shrt_cmp (STR_RULEID, token) )
    {
      ruleID = (int)strtol(event->data + t[i + 1].start, &endptr, 10);
      i++; v++;
    }
  }

  // Loop through the events we respond too
  if ( v == 5 )
  {
    for ( int i = 0; i <= HKATOP; i++ )
    {
      if ( hik_alert[i].devId == devId )
      {
        // Check if we have a match and we light up zones accordingly
        if ( (hik_alert[i].eventType == eventType) && (hik_alert[i].ivmsChannel == ivmsChannel) && (hik_alert[i].ruleID == ruleID) && hik_alert[i].zone )
        {
#if defined (CONFIG_DEBUG)
          F_LOGI(true, true, LC_GREY, "devId: %2d, eventType: %2d, ivmsChannel: %2d, ruleID: %2d, zone: %2d", devId, eventType, ivmsChannel, ruleID, hik_alert[i].zone);
#endif
          // Set security light?
          if ( hik_alert[i].sec_light && lightcheck.isdark )
          {
            F_LOGI(true, true, LC_WHITE, "Switching on external security light");
            // GPIO controlled light
            if ( hik_alert[i].sec_light < 0xff )
            {
              set_light(SECURITY_LIGHT, true, hik_alert[i].sec_light);
            }
#if defined(CONFIG_CCTV)
            // HikVision MQTT controlled light
            else if ( hik_alert[i].sec_light == 0xff )
            {
              toggle_cctv_daynight(devId);
            }
#endif
          }

          // Iterate through zones
          for ( int z = 0; z < 4; z++ )
          {
            if ( BTST(hik_alert[i].zone, (1 << z)) )
            {
              //F_LOGI(true, true, LC_WHITE, "devId: %2d, eventType: %2d, ivmsChannel: %2d, ruleID: %2d, zone: %2d", devId, eventType, ivmsChannel, ruleID, hik_alert[i].zone);
              setZone(z, hik_alert[i].mask, hik_alert[i].color, hik_alert[i].time);
            }
          }
        }
      }
      else if ( hik_alert[i].devId > devId )
      {
        break;
      }
    }
  }
}

void proc_ev_dev_network (esp_mqtt_event_handle_t event)
{
  /* Process JSON Data */
  char token[64] __attribute__ ((aligned(4))) = {};
  char iface[32] __attribute__ ((aligned (4))) = {};
  char status[64] __attribute__ ((aligned (4))) = {};
  int i, r, z = 0;
  jsmn_parser p;
  jsmntok_t t[12];

  //F_LOGI(true, true, LC_WHITE, "%.*s -> %.*s", event->topic_len, event->topic, event->data_len, event->data);

  jsmn_init (&p);
  r = jsmn_parse(&p, event->data, event->data_len, t, sizeof (t) / sizeof (t[0]));

  for ( i = 1; i < r; i++ )
  {
    snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);

    if ( !shrt_cmp ("iface", token) )
    {
      i++;
      sprintf(iface, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
    }
    else if ( !shrt_cmp ("status", token) )
    {
      i++;
      sprintf(status, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
    }
  }

  F_LOGV(true, true, LC_GREEN, "iface: %s, status: %s", iface, status);
  if ( !str_cmp ("BT-PPPoE", iface) )
  {
    z = 4;
  }
  else if ( !str_cmp ("PN-PPPoE", iface) )
  {
    z = 5;
  }

  if ( !shrt_cmp("connected", status) )
  {
    setZone (z, OV_MASK_PAUSED | OV_MASK_DAY, CRGBMediumWhite, 75);
  }
  else
  {
    setZone (z, OV_MASK_PAUSED | OV_MASK_DAY | OV_MASK_NIGHT, CRGBDarkRed, 2200);
  }
}

#define BUF_SIZE    2048
char jsonEngPacket[BUF_SIZE+1];
char jsonBasPacket[BUF_SIZE+1];
uint16_t engPacketLength = 0;
uint16_t basPacketLength = 0;
void proc_ev_radar (esp_mqtt_event_handle_t event)
{
  char token[64] __attribute__ ((aligned(4))) = {};
  char area[32] = {};
  uint16_t mov[9] = {};
  uint16_t sta[9] = {};
  char *endptr;

  int r = 0;
  jsmn_parser p;
  jsmntok_t t[30];
  //jsmntok_t *g;

  jsmn_init (&p);
  r = jsmn_parse(&p, event->data, event->data_len, t, sizeof (t) / sizeof (t[0]));

  bool eng_pkt = false;
  uint16_t i = 1;
  while ( i < r )
  {
    snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);

    if ( !shrt_cmp("area", token) )
    {
      sprintf(area, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
      if ( str_cmp ("caravan", area) )
      //if ( str_cmp("test-esp32s3", area) )
      {
        return;
      }
      F_LOGV(true, true, LC_GREEN, "data: %.*s (%d)", event->data_len, event->data, r);
      i++;
    }
    else if ( !shrt_cmp("eng", token) )
    {
      // Flag as engineering data packet
      eng_pkt = true;

      // Ensure this is an object
      if ( t[i].type == JSMN_OBJECT )
      {
        // Number of objects
        uint16_t objs = t[i++].size;

        for ( uint16_t v = 0; v < objs; v++ )
        {
          if ( !shrt_cmp ("mov", event->data + t[i].start) )
          {
            if ( t[++i].type == JSMN_ARRAY )
            {
              // Store the length of the array
              uint16_t elements = t[i++].size;

              // Create a code friendly pointer to the elements
              //g = &t[i];

              for ( uint16_t z = 0; z < elements; z++ )
              {
                mov[z] = (uint16_t)strtol(event->data + t[i].start, &endptr, 10);
                i++;
              }
            }
          }
          else if ( !shrt_cmp ("sta", event->data + t[i].start) )
          {
            if ( t[++i].type == JSMN_ARRAY )
            {
              // Store the length of the array
              uint16_t elements = t[i++].size;

              // Create a code friendly pointer to the elements
              //g = &t[i];

              for ( uint16_t z = 0; z < elements; z++ )
              {
                sta[z] = (uint16_t)strtol(event->data + t[i].start, &endptr, 10);
                i++;
              }
            }
          }
        }
      }
    }
    else if ( !shrt_cmp ("mov", token) )
    {
      if ( t[i].type == JSMN_OBJECT )
      {
        // Number of objects
        uint16_t objs = t[i++].size;

        for ( uint16_t v = 0; v < objs; v++ )
        {
          if ( !shrt_cmp ("dst", event->data + t[i].start) )
          {
            mov[0] = (uint16_t)strtol (event->data + t[++i].start, &endptr, 10);
            i++;
          }
          else if ( !shrt_cmp ("nrg", event->data + t[i].start) )
          {
            mov[1] = (uint16_t)strtol (event->data + t[++i].start, &endptr, 10);
            i++;
          }
        }
      }
    }
    else if ( !shrt_cmp ("sta", token) )
    {
      if ( t[i].type == JSMN_OBJECT )
      {
        // Number of objects
        uint16_t objs = t[i++].size;

        for ( uint16_t v = 0; v < objs; v++ )
        {
          if ( !shrt_cmp ("dst", event->data + t[i].start) )
          {
            sta[0] = (uint16_t)strtol (event->data + t[++i].start, &endptr, 10);
            i++;
          }
          else if ( !shrt_cmp ("nrg", event->data + t[i].start) )
          {
            sta[1] = (uint16_t)strtol (event->data + t[++i].start, &endptr, 10);
            i++;
          }
        }
      }
    }
  }

  if ( eng_pkt )
  {
    F_LOGV(true, true, LC_GREY, "%15s: %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d / %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d ",
      area, mov[0], mov[1], mov[2], mov[3], mov[4], mov[5], mov[6], mov[7], mov[8], sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], sta[6], sta[7], sta[8]);

    // For graphing, just make a copy of the incoming JSON data. No use dissecting and reassembling a new one
    memcpy(&jsonEngPacket, event->data, event->data_len);
    engPacketLength = event->data_len;
  }
  else
  {
    F_LOGV(true, true, LC_GREY, "%15s; mov: %3d, %3d, sta: %3d, %3d", area, mov[0], mov[1], sta[0], sta[1]);
    // For graphing, just make a copy of the incoming JSON data. No use dissecting and reassembling a new one
    memcpy(&jsonBasPacket, event->data, event->data_len);
    basPacketLength = event->data_len;
  }

  // Zones not implemented in all cases
  if ( sta[0] > 40 && sta[1] > 100 ) // never going to be over 100
  {
    setZone (6, OV_MASK_PAUSED | OV_MASK_DAY | OV_MASK_NIGHT, CRGBOrange, 50);
  }
  if ( mov[0] > 75 && mov[1] > 35 )
  {
    setZone (7, OV_MASK_PAUSED | OV_MASK_DAY | OV_MASK_NIGHT, CRGBRed, 5000);
  }
}

IRAM_ATTR int getEngDataJsonString (char **buf)
{
  *(buf) = jsonEngPacket;
  return engPacketLength;
}

void proc_ev_lux (esp_mqtt_event_handle_t event)
{
  char token[64] __attribute__ ((aligned(4))) = {};
  char area[32] = {};
  char *endptr;

  int i, r = 0;
  jsmn_parser p;
  jsmntok_t t[15];

  jsmn_init (&p);
  r = jsmn_parse(&p, event->data, event->data_len, t, sizeof (t) / sizeof (t[0]));

  for ( i = 1; i < r; i++ )
  {
    snprintf(token, 63, "%.*s", t[i].end - t[i].start, event->data + t[i].start);

    if ( !shrt_cmp ("area", token) )
    {
      i++;
      sprintf(area, "%.*s", t[i].end - t[i].start, event->data + t[i].start);
      if ( str_cmp ("caravan", area) )
      {
        return;
      }
    }
    else if ( !shrt_cmp ("light", token) )
    {
      uint16_t light = (uint16_t)strtol (event->data + t[i + 1].start, &endptr, 10);
      F_LOGD(true, true, LC_RED, "light level = %d", light);
      lightcheck.lux_level = light;
      i++;
    }
  }
}

/*********************************************************************************/
/* subscribed: The string used to subscribe                                      */
/* topic: The topic string we received                                           */
/* topic_len: The length of the received topic string                            */
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR int topic_cmp(const char *subscribed, const char *topic, uint16_t topic_len)
#else
uint16_t topic_cmp(const char *subscribed, const char *topic, uint16_t topic_len)
#endif
{
  int16_t vld = 1, ps = 0;

  // Iterate over both strings
  for (int tpos = 0, spos = 0; vld && tpos < topic_len; tpos++ )
  {
    // Case insensitive character comparison.
    if ( (subscribed[spos] | 32) - (topic[tpos] | 32) )
    {
      // No match, so first check if we are in a wildcard search
      if ( !ps )
      {
        // Not in a wildcard search, check for 'Single topic' wildcard
        if ( subscribed[spos] == '+' )
        {
          // Flag entering single topic wildcard comparison
          ps = 1;

          // Move past the wildcard character
          spos++;
        }
        // Check for Complete (end of line) wildcard substitution
        else if ( subscribed[spos] == '#' )
        {
          break;
        }
        // Not a wildcard
        else
        {
          // Topics don't match, set the return value and terminate the search
          vld = 0;
          break;
        }
      }
      // We are in a 'single topic' wildcard, check for end of topic or return from comparison
      else
      {
        // Check for end of topic
        if ( topic[tpos] == '/' )
        {
          // Simples
          ps = 0;
        }
        // A little more work if we reach EOL
        else if ( tpos == (topic_len - 1) )
        {
          // End on correct EOL 'single topic' wildcard
          if ( subscribed[spos] == 0 )
          {
            break;
          }
          // Our 'single topic' wildcard continues, but our comparison topic ends here.
          else if ( subscribed[spos] == '/' )
          {
            vld = 0;
            break;
          }
          // No 'else'. We have all our bases covered.
        }
      }
    }
    else
    {
      spos++;
    }
  }

  return(vld);
}

/// @brief
/// @param event
/// @return
void proc_mqtt_data (esp_mqtt_event_handle_t event)
{
  uint16_t dev;

  //F_LOGI(true, true, LC_BRIGHT_WHITE, "proc_mqtt_data: %.*s -> %.*s", event->topic_len, event->topic, event->data_len, event->data);
  for ( dev = dev_Light; dev < num_devices; dev++ )
  {
    //F_LOGI(true, true, LC_BRIGHT_BLUE, "proc_mqtt_data: %s -> %.*s", MQTT_server_cfg[dev].Topic_sub, event->topic_len, event->topic);
    if ( topic_cmp(MQTT_server_cfg[dev].Topic_sub, event->topic, event->topic_len) == 1 )
    {
      F_LOGD(true, true, LC_MAGENTA, "proc_mqtt_data: %.*s -> %.*s", event->topic_len, event->topic, event->data_len, event->data);
      switch ( (mqtt_devices_t)dev )
      {
        case dev_Light:
          // Obey only when we are a slave
          //if (BTST(control_vars.bitflags, DISP_BF_SLAVE))
          {
            int sw_state = on_off (event->data_len, event->data);
            set_light (SECURITY_LIGHT, sw_state, (sw_state == 1?50:0));
          }
          break;
        case dev_LEDs:
          // Obey only when we are a slave
          if ( MQTT_Client_Cfg.Slave == true )
          {
            proc_ev_dev_leds(event);
          }
          break;
        case dev_Pattern:
          // Obey only when we are a slave
          if ( BTST(control_vars.bitflags, DISP_BF_SLAVE) )
          {
            char *pattern = (char *)pvPortMalloc (event->data_len + 1);
            if ( pattern == NULL )
            {
              F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating %d bytes for 'pattern'", (event->data_len + 1));
            }
            else
            {
              memcpy (pattern, event->data, event->data_len);
              pattern[event->data_len] = 0x0;
              getPatternByName (pattern, true);
              vPortFree (pattern);
            }
          }
          break;
        case dev_Connect:
          break;
        case dev_Discon:
          break;
        case dev_hikAlarm:
          proc_ev_dev_hikAlarm (event);
          break;
        case dev_Network:
          proc_ev_dev_network (event);
          break;
        case dev_Radar:
          proc_ev_radar (event);
          break;
        case dev_Lux:
          proc_ev_lux (event);
          break;
        default:
          break;
      }
      // Topic found, end processing
      break;
    }
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void start_mqtt_client(esp_mqtt_client_handle_t *MQTTClient)
{
  esp_err_t err = ESP_FAIL;
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  esp_mqtt_client_config_t mqtt_cfg = {
      //.event_handle    = mqtt_event_handler,
      //.host            = resolve_host(host),
      .uri             = MQTT_Client_Cfg.Uri,
      .port            = MQTT_Client_Cfg.Port,
      .client_id       = MQTT_Client_Cfg.Client_ID,
      .username        = MQTT_Client_Cfg.Username,
      .password        = MQTT_Client_Cfg.Password,
      //.transport       = ssl ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP,
      //.cert_pem        = server_cert,
      //.client_cert_pem = client_cert,
      //.client_key_pem  = client_key,
      //.user_context    = 0,
      .lwt_topic       = get_lwt_topic (),
      .lwt_msg         = "Disconnected",
      .lwt_retain      = 1,
      .keepalive       = MQTT_Client_Cfg.Keep_Alive
  };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

  // Attempt to initialise MQTT client
  if ( strlen(mqtt_cfg.uri) > 0 )
  {
    *MQTTClient = esp_mqtt_client_init(&mqtt_cfg);
  }

  // If MQTT was successfully initialised then we attempt to start the client
  if ( *MQTTClient )
  {
    err = esp_mqtt_client_start(*MQTTClient);
  }

  if ( err )
  {
    F_LOGE(true, true, LC_RED, "esp_mqtt_client_start, %s", esp_err_to_name (err));
    stop_mqtt_client(*MQTTClient);
    *MQTTClient = NULL;
  }
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void stop_mqtt_client(esp_mqtt_client_handle_t MQTTClient)
{
  if ( MQTTClient )
  {
    esp_err_t err = esp_mqtt_client_stop(MQTTClient);
    if ( err != ESP_OK )
    {
      F_LOGE(true, true, LC_RED, "Error stopping MQTT client: %s", esp_err_to_name(err));
    }
    else
    {
      F_LOGW(true, true, LC_YELLOW, "MQTT client stopped");
    }

    err = esp_mqtt_client_destroy(MQTTClient);
    if ( err != ESP_OK )
    {
      F_LOGE(true, true, LC_RED, "Error destroying MQTT client: %s", esp_err_to_name(err));
    }
    else
    {
      F_LOGW(true, true, LC_YELLOW, "MQTT client destroyed");
    }
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void mqtt_restart(void)
{
  if ( mqtt_client )
  {
    stop_mqtt_client(mqtt_client);
    mqtt_client = NULL;
  }

  if ( !mqtt_client )
  {
    start_mqtt_client((esp_mqtt_client_handle_t *)&mqtt_client);
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  esp_mqtt_client_handle_t *MQTTClient = (esp_mqtt_client_handle_t *)arg;

  if ( *MQTTClient == NULL )
  {
    start_mqtt_client(MQTTClient);
  }
}
static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  esp_mqtt_client_handle_t *MQTTClient = (esp_mqtt_client_handle_t *)arg;

  if ( *MQTTClient )
  {
    stop_mqtt_client(*MQTTClient);
    *MQTTClient = NULL;
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void init_mqtt_client(void)
{
  mqtt_event_group = xEventGroupCreate();
  mqtt_send_queue  = xQueueCreate(12, sizeof(mqtt_mesg_t *));

  getMqttConfig();
  // Need this before network config, so moved to app_main.cpp
  _get_nvs_mqtt_config();

  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, (void *)&mqtt_client);
  esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, (void *)&mqtt_client);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
bool mqtt_check_connected(void)
{
  bool retval = false;

  if ( mqtt_event_group != NULL )
  {
    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    retval = BTST(bits, MQTT_CONNECTED);
  }

  return retval;
}

/* A realloc-based function by Albert Chan <albertmcchan@yahoo.com>, sent to me
 * privately, and reproduced here with Albert's permission as public domain
 * code, with my only edit being to qualify the parameters as const.
 */
char *replace_smart (const char *str, const char *sub, const char *rep)
{
  size_t slen     = strlen (sub);
  size_t rlen     = strlen (rep);
  size_t size     = strlen (str) + 1;
  size_t diff     = rlen - slen;
  size_t capacity = (diff > 0 && slen)?2 * size:size;
  char *buf       = (char *)pvPortMalloc(capacity);
  char *find, *b = buf;

  if ( buf == NULL )
  {
    F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating %d bytes for 'buf'", capacity);
  }
  else
  {
    if ( b == NULL )
    {
      return NULL;
    }

    if ( slen == 0 )
    {
      return (char *)memcpy (b, str, size);
    }

    while ( (find = strstr (str, sub)) )
    {
      if ( (size += diff) > capacity )
      {
        char *ptr = (char *)realloc (buf, capacity = 2 * size);
        if ( ptr == NULL )
        {
          vPortFree (buf);
          return NULL;
        }
        b = ptr + (b - buf);
        buf = ptr;
      }

      memcpy (b, str, find - str); /* copy up to occurrence */
      b += find - str;
      memcpy (b, rep, rlen);       /* add replacement */
      b += rlen;
      str = find + slen;
    }

    memcpy (b, str, size - (b - buf));
    b = (char *)realloc(buf, size);         /* trim to size */
  }
  return b?b:buf;
}

char *_parse_mqtt_string (const char *str)
{
  char *tmp_str = replace_smart (str, "%c", MQTT_Client_Cfg.Client_ID);

  return tmp_str;
}

char *get_lwt_topic (void)
{
  return _parse_mqtt_string (MQTT_server_cfg[dev_Discon].Topic_pub);
}

void mqtt_publish (char *topic, const char *data, int len, int qos, int retain)
{
  if ( mqtt_send_queue == NULL )
  {
    return;
  }

  mqtt_mesg_t *mesg = (mqtt_mesg_t *)pvPortMalloc(sizeof(mqtt_mesg_t));
  if ( mesg == NULL )
  {
    F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating %d bytes for 'mesg'", sizeof(mqtt_mesg_t));
  }
  else
  {
    mesg->topic   = _parse_mqtt_string (topic);
    mesg->data    = strndup (data, len);
    mesg->len     = len;
    mesg->qos     = qos;
    mesg->retain  = retain;
    xQueueSendToBack (mqtt_send_queue, &mesg, 0);
    /*
    if (mqtt_check_connected ())
    {
      if ((msg_id = esp_mqtt_client_publish(mqtt_client, tmpTopic, data, len, qos, retain)) == -1)
      {
        F_LOGE(true, true, LC_GREEN, "esp_mqtt_client_publish: %s -> %s", tmpTopic, data);
      }
    }
    */
  }
}

int mqtt_subscribe (char *topic, int qos)
{
  char *tmpTopic = NULL;
  int msg_id = 0;

  tmpTopic = _parse_mqtt_string(topic);

  if ( mqtt_check_connected() )
  {
    if ( (msg_id = esp_mqtt_client_subscribe(mqtt_client, tmpTopic, qos)) == -1 )
    {
      F_LOGI(true, true, LC_RED, "esp_mqtt_client_subscribe: %s", tmpTopic);
    }
  }

  if ( tmpTopic )
  {
    vPortFree(tmpTopic);
    tmpTopic = NULL;
  }

  return msg_id;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

int mqttSubscribe (enum mqtt_devices device)
{
  F_LOGI(true, true, LC_GREY, "Subscribing to \"%s\"", MQTT_server_cfg[device].Topic_sub);
  return mqtt_subscribe(MQTT_server_cfg[device].Topic_sub, MQTT_server_cfg[device].QoS);
}

// Publish on our status topic
void mqttPublish (enum mqtt_devices device, const char *format, ...)
{
  char tmpBuf[MQTT_BUFSIZE];
  va_list va_args;

  va_start(va_args, format);

  vsnprintf(tmpBuf, MQTT_BUFSIZE, format, va_args);
  F_LOGV(true, true, LC_GREEN, "Topic = %s (%s)", MQTT_server_cfg[device].Topic_pub, tmpBuf);
  mqtt_publish(MQTT_server_cfg[device].Topic_pub, tmpBuf, strlen (tmpBuf), MQTT_server_cfg[device].QoS, MQTT_server_cfg[device].Retained);

  va_end(va_args);
}

// Publish on our control topic
void mqttControl(enum mqtt_devices device, const char *format, ...)
{
  if ( MQTT_Client_Cfg.Master == true )
  {
    char tmpBuf[MQTT_BUFSIZE];
    va_list va_args;

    va_start (va_args, format);

    vsnprintf (tmpBuf, MQTT_BUFSIZE, format, va_args);
    F_LOGV(true, true, LC_GREEN, "Topic = %s (%s)", MQTT_server_cfg[device].Topic_sub, tmpBuf);
    mqtt_publish (MQTT_server_cfg[device].Topic_sub, tmpBuf, strlen (tmpBuf), MQTT_server_cfg[device].QoS, MQTT_server_cfg[device].Retained);

    va_end (va_args);
  }
}

void mqtt_send_task (void *arg)
{
  int msg_id;
  mqtt_mesg_t *mesg = NULL;

  CHECK_ERROR_CODE (esp_task_wdt_add (NULL), ESP_OK);
  CHECK_ERROR_CODE (esp_task_wdt_status (NULL), ESP_OK);

  for ( ;;)
  {
    if ( xQueueReceive (mqtt_send_queue, &mesg, pdMS_TO_TICKS (100)) == pdTRUE )
    {
      F_LOGV(true, true, LC_GREY, "(MQTT Send) T: %s, D: %s, (qos: %d, retain: %d)", mesg->topic, mesg->data, mesg->qos, mesg->retain);

      if ( mqtt_check_connected () )
      {
        if ( (msg_id = esp_mqtt_client_publish(mqtt_client, mesg->topic, mesg->data, mesg->len, mesg->qos, mesg->retain)) == -1 )
        {
          F_LOGE(true, true, LC_GREEN, "esp_mqtt_client_publish: %s -> %s", mesg->topic, mesg->data);
        }
      }

      vPortFree (mesg->topic);
      vPortFree (mesg->data);
      vPortFree (mesg);
      mesg = NULL;
    }

    // Reset the WDT
    CHECK_ERROR_CODE (esp_task_wdt_reset (), ESP_OK);
  }
}

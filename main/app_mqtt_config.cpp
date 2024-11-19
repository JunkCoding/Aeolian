
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>


#include "app_configs.h"
#include "app_mqtt_settings.h"
#include "app_mqtt_config.h"
#include "app_mqtt.h"
#include "app_main.h"
#include "app_utils.h"
#include "app_flash.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
MQTT_client_cfg_t MQTT_Client_Cfg;
MQTT_server_cfg_t MQTT_server_cfg[num_devices];

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
const Config_Keyword_t ICACHE_RODATA_ATTR MqttConfig_Keywords[]
ICACHE_RODATA_ATTR STORE_ATTR = {
  {{GRP_LIGHT + TOPIC_SUB,     TEXT,    0, 0}, "cLight"},
  {{GRP_LIGHT + TOPIC_PUB,     TEXT,    0, 0}, "sLight"},

  {{GRP_LEDS + TOPIC_SUB,      TEXT,    0, 0}, "cLEDs"},
  {{GRP_LEDS + TOPIC_PUB,      TEXT,    0, 0}, "sLEDs"},

  {{GRP_PATTERN + TOPIC_SUB,   TEXT,    0, 0}, "cPattern"},
  {{GRP_PATTERN + TOPIC_PUB,   TEXT,    0, 0}, "sPattern"},

  {{GRP_CONNECT + TOPIC_SUB,   TEXT,    0, 0}, "cConnect"},
  {{GRP_CONNECT + TOPIC_PUB,   TEXT,    0, 0}, "sConnect"},

  {{GRP_DISCON + TOPIC_SUB,    TEXT,    0, 0}, "cDiscon"},
  {{GRP_DISCON + TOPIC_PUB,    TEXT,    0, 0}, "sDiscon"},

  {{GRP_HIKALARM + TOPIC_SUB,  TEXT,    0, 0}, "cHikAlarm"},
  {{GRP_HIKALARM + TOPIC_PUB,  TEXT,    0, 0}, "sHikAlarm"},

  {{GRP_NETWORK + TOPIC_SUB,   TEXT,    0, 0}, "cNetwork"},
  {{GRP_NETWORK + TOPIC_PUB,   TEXT,    0, 0}, "sNetwork"},

  {{GRP_RADAR + TOPIC_SUB,     TEXT,    0, 0}, "cRadar"},
  {{GRP_RADAR + TOPIC_PUB,     TEXT,    0, 0}, "sRadar"},

  {{GRP_LUX + TOPIC_SUB,       TEXT,    0, 0}, "cLux"},
  {{GRP_LUX + TOPIC_PUB,       TEXT,    0, 0}, "sLux"},

  {{GRP_NETWORK + TOPIC_SUB,   TEXT,    0, 0}, "cNetwork"},
  {{GRP_NETWORK + TOPIC_PUB,   TEXT,    0, 0}, "sNetwork"},
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static int mqtt_get_config (int id, char **str, int *value);
static int mqtt_compare_config (int id, char *str, int value);
static int mqtt_update_config (int id, char *str, int value);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int mqtt_get_config (int id, char **str, int *value)
{
  F_LOGV(true, true, LC_GREY, "--------------------------------------------");
  F_LOGV(true, true, LC_GREY, "id: %x, str: \"%s\", value: %d", id, (char *)str, (int)*value);

  int grp = id & 0xF0;

  if ( grp >= GRP_FIRST && grp <= GRP_LAST )
  {
    int dev = (grp >> 4) - 1;

    switch ( id & 0x0F )
    {
      case DEV_NAME:
        *str = MQTT_server_cfg[dev].Name;
        return true;
      case TOPIC_SUB:
        *str = MQTT_server_cfg[dev].Topic_sub;
        return true;
      case TOPIC_PUB:
        *str = MQTT_server_cfg[dev].Topic_pub;
        return true;
      case ENABLE_PUBLISH:
        *value = MQTT_server_cfg[dev].Enable_publish;
        return true;
      case RETAINED:
        *value = MQTT_server_cfg[dev].Retained;
        return true;
      case QOS:
        *value = MQTT_server_cfg[dev].QoS;
        return true;
    }
  }

  // id not handled here, nothing to compare
  F_LOGV(true, true, LC_GREY, "Not handled by mqtt_get_Config");

  return false;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// return
//    1       value doesn't change configuration
//    0       value  changed configuration
//   -1       id not handled here, nothing to compare

int mqtt_compare_config (int id, char *str, int value)
{
  int retval = -1;

  F_LOGV(true, true, LC_GREY, "0x%02x \"%s\" %d", id, S (str), value);

  int grp = id & 0xF0;

  if ( grp >= GRP_FIRST && grp <= GRP_LAST )
  {
    int dev = (grp >> 4) - 1;

    switch ( id & 0x0F )
    {
      case DEV_NAME:
        retval = strncmp (MQTT_server_cfg[dev].Name, str, sizeof(MQTT_server_cfg[dev].Name)) == 0?1:0;
        break;
      case TOPIC_SUB:
        retval = strncmp (MQTT_server_cfg[dev].Topic_sub, str, sizeof(MQTT_server_cfg[dev].Topic_sub)) == 0?1:0;
        break;
      case TOPIC_PUB:
        retval = strncmp (MQTT_server_cfg[dev].Topic_pub, str, sizeof(MQTT_server_cfg[dev].Topic_pub)) == 0?1:0;
        break;
      case ENABLE_PUBLISH:
        retval = MQTT_server_cfg[dev].Enable_publish == value?1:0;
        break;
      case RETAINED:
        retval = MQTT_server_cfg[dev].Retained == value?1:0;
        break;
      case QOS:
        retval = MQTT_server_cfg[dev].QoS == value?1:0;
      default:
        break;
    }
  }

  // id not handled here, nothing to compare
  F_LOGV(true, true, LC_GREY, "retval = %d", retval);

  return retval;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int mqtt_update_config (int id, char *str, int value)
{
  F_LOGV(true, true, LC_GREY, "0x%02x \"%s\" %d", id, S (str), value);

  int grp = id & 0xF0;

  if ( grp >= GRP_FIRST && grp <= GRP_LAST )
  {
    int dev = (grp >> 4) - 1;

    switch ( id & 0x0F )
    {
      case DEV_NAME:
        strlcpy(MQTT_server_cfg[dev].Name, str, sizeof (MQTT_server_cfg[dev].Name));
        break;
      case TOPIC_SUB:
        strlcpy(MQTT_server_cfg[dev].Topic_sub, str, sizeof (MQTT_server_cfg[dev].Topic_sub));
        break;
      case TOPIC_PUB:
        strlcpy(MQTT_server_cfg[dev].Topic_pub, str, sizeof (MQTT_server_cfg[dev].Topic_pub));
        break;
      case ENABLE_PUBLISH:
        MQTT_server_cfg[dev].Enable_publish = value;
        break;
      case RETAINED:
        MQTT_server_cfg[dev].Retained = value;
        break;
      case QOS:
        MQTT_server_cfg[dev].QoS = value;
        break;
      default:
        return 0;
    }

    return 1;
  }

  // id not handled here, nothing to update
  F_LOGV(true, true, LC_BRIGHT_GREEN, "Not handled by mqtt_update_Config");

  return -1;
}

int _set_mqtt_setting (char *buf, int bufsize, char *param, char *value, int setting)
{
  tNumber saveType = TYPE_NONE;

  F_LOGV(true, true, LC_BRIGHT_YELLOW, "_set_mqtt_setting: param: %s, value: %s", param, value);

  switch ( (mqtt_type_t)setting )
  {
    case MQTT_CLIENT_ID:
      if ( strcmp(value, MQTT_Client_Cfg.Client_ID) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(MQTT_Client_Cfg.Client_ID, value, SSID_STRLEN);
        // Restart the client, if necessary
        mqtt_restart();
      }
      break;
    case MQTT_URI:
      if ( strcmp(value, MQTT_Client_Cfg.Uri) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(MQTT_Client_Cfg.Uri, value, SSID_STRLEN);
        // Restart the client, if necessary
        mqtt_restart();
      }
      break;
    case MQTT_USERNAME:
      if ( strcmp(value, MQTT_Client_Cfg.Username) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(MQTT_Client_Cfg.Username, value, SSID_STRLEN);
        // Restart the client, if necessary
        mqtt_restart();
      }
      break;
    case MQTT_PASSWORD:
      if ( strcmp(value, MQTT_Client_Cfg.Password) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(MQTT_Client_Cfg.Password, value, SSID_STRLEN);
        // Restart the client, if necessary
        mqtt_restart();
      }
      break;
    case MQTT_PORT:
      {
        uint32_t port = atoi(value);
        if ( MQTT_Client_Cfg.Port != port )
        {
          saveType = TYPE_U32;
          MQTT_Client_Cfg.Port = port;
          // Restart the client, if necessary
          mqtt_restart();
        }
      }
      break;
    case MQTT_KEEPALIVE:
      {
        bool ka = atoi(value);
        if ( MQTT_Client_Cfg.Keep_Alive != ka )
        {
          saveType = TYPE_U8;
          MQTT_Client_Cfg.Keep_Alive = ka;
        }
      }
      break;
    case MQTT_MASTER:
      {
        bool master = atoi(value);
        if ( MQTT_Client_Cfg.Master != master )
        {
          saveType = TYPE_U8;
          MQTT_Client_Cfg.Master = master;
        }
      }
      break;
    case MQTT_SLAVE:
      {
        bool slave = atoi(value);
        if ( MQTT_Client_Cfg.Slave != slave )
        {
          saveType = TYPE_U8;
          MQTT_Client_Cfg.Slave = slave;
        }
      }
      break;
  }

  esp_err_t err = ESP_FAIL;
  if ( saveType == TYPE_STR )
  {
    err = save_nvs_str(NVS_MQTT_CONFIG, param, value);
  }
  else if ( saveType != TYPE_NONE )
  {
    err = save_nvs_num(NVS_MQTT_CONFIG, param, value, saveType);
  }

  return(snprintf(buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}

int _get_mqtt_setting (char *buf, int bufsize, int setting)
{
  uint16_t len = 0;

  F_LOGV(true, true, LC_BRIGHT_YELLOW, "_get_mqtt_setting: setting = %d", setting);

  switch ( (mqtt_type_t)setting )
  {
    case MQTT_CLIENT_ID:
      len = snprintf(buf, bufsize, "%s", MQTT_Client_Cfg.Client_ID);
      break;
    case MQTT_URI:
      len = snprintf(buf, bufsize, "%s", MQTT_Client_Cfg.Uri);
      break;
    case MQTT_USERNAME:
      len = snprintf(buf, bufsize, "%s", MQTT_Client_Cfg.Username);
      break;
    case MQTT_PASSWORD:
      len = snprintf(buf, bufsize, "%s", MQTT_Client_Cfg.Password);
      break;
    case MQTT_PORT:
      len = snprintf(buf, bufsize, "%lu", MQTT_Client_Cfg.Port);
      break;
    case MQTT_KEEPALIVE:
      len = snprintf(buf, bufsize, "%d", MQTT_Client_Cfg.Keep_Alive);
      break;
    case MQTT_MASTER:
      len = snprintf(buf, bufsize, "%d", MQTT_Client_Cfg.Master);
      break;
    case MQTT_SLAVE:
      len = snprintf(buf, bufsize, "%d", MQTT_Client_Cfg.Slave);
      break;
  }

  return len;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void getMqttConfig (void)
{
  // F_LOGV(true, true, LC_BRIGHT_GREEN, "user_mqtt_configure");

  uint16_t dev;

  for ( dev = dev_Light; dev < num_devices; dev++ )
  {
    int dev_id = ((dev + 1) << 4);
    // F_LOGV(true, true, LC_BRIGHT_GREEN, "user_mqtt_configure %d 0x%02x", dev, dev_id);

    _config_get_bool(dev_id + ENABLE_PUBLISH, &MQTT_server_cfg[dev].Enable_publish);
    _config_get_bool(dev_id + RETAINED, &MQTT_server_cfg[dev].Retained);
    _config_get_uint8(dev_id + QOS, &MQTT_server_cfg[dev].QoS);

    _config_get(dev_id + DEV_NAME, MQTT_server_cfg[dev].Name, sizeof(MQTT_server_cfg[dev].Name));
    _config_get(dev_id + TOPIC_SUB, MQTT_server_cfg[dev].Topic_sub, sizeof(MQTT_server_cfg[dev].Topic_sub));
    _config_get(dev_id + TOPIC_PUB, MQTT_server_cfg[dev].Topic_pub, sizeof(MQTT_server_cfg[dev].Topic_pub));

    F_LOGV(true, true, LC_GREY, " 0x%02x, dev: %s", dev, MQTT_server_cfg[dev].Name);
    F_LOGV(true, true, LC_GREY, "       sub: %s", MQTT_server_cfg[dev].Topic_sub);
    F_LOGV(true, true, LC_GREY, "       pub: %s", MQTT_server_cfg[dev].Topic_pub);
    F_LOGV(true, true, LC_GREY, "       cfg: %d %d %d", MQTT_server_cfg[dev].Enable_publish, MQTT_server_cfg[dev].Retained, MQTT_server_cfg[dev].QoS);
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void _get_nvs_mqtt_config(void)
{
  size_t strlen;
  nvs_handle handle;

  esp_err_t err = nvs_open(NVS_MQTT_CONFIG, NVS_READONLY, &handle);
  if ( err == ESP_OK )
  {
    strlen = MQTT_CLIENT_STRLEN_NAME;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_NAME, MQTT_Client_Cfg.Name, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Name[0] = 0x0;
    }

    strlen = MQTT_CLIENT_STRLEN_SERVER;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_SERVER, MQTT_Client_Cfg.Server, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Server[0] = 0x0;
    }

    strlen = MQTT_CLIENT_STRLEN_URI;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_URI, MQTT_Client_Cfg.Uri, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Uri[0] = 0x0;
    }

    strlen = MQTT_CLIENT_STRLEN_ID;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_ID, MQTT_Client_Cfg.Client_ID, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Client_ID[0] = 0x0;
    }

    strlen = MQTT_CLIENT_STRLEN_USERNAME;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_USERNAME, MQTT_Client_Cfg.Username, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Username[0] = 0x0;
    }

    strlen = MQTT_CLIENT_STRLEN_PASSWORD;
    if ( nvs_get_str(handle, STR_MQTT_CLIENT_PASSWORD, MQTT_Client_Cfg.Password, &strlen) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Password[0] = 0x0;
    }

    // Validation done later
    uint32_t port;
    if ( nvs_get_u32(handle, STR_MQTT_CLIENT_PORT, &port) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Port = 1883;
    }
    else
    {
      MQTT_Client_Cfg.Port = port;
    }

    uint8_t val;
    if ( nvs_get_u8(handle, STR_MQTT_CLIENT_MASTER, &val) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Master = false;
    }
    else
    {
      MQTT_Client_Cfg.Master = val;
    }

    if ( nvs_get_u8(handle, STR_MQTT_CLIENT_SLAVE, &val) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Slave = false;
    }
    else
    {
      MQTT_Client_Cfg.Slave = val;
    }

    if ( nvs_get_u8(handle, STR_MQTT_CLIENT_KEEPALIVE, &val) == ESP_ERR_NVS_NOT_FOUND )
    {
      MQTT_Client_Cfg.Keep_Alive = false;
    }
    else
    {
      MQTT_Client_Cfg.Keep_Alive = val;
    }

    nvs_close(handle);
  }

  F_LOGW(true, true, LC_GREY, "MQTT_Client Client_ID  : %s", MQTT_Client_Cfg.Client_ID);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Uri        : %s", MQTT_Client_Cfg.Uri);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Username   : %s", MQTT_Client_Cfg.Username);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Password   : %s", MQTT_Client_Cfg.Password);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Port       : %d", MQTT_Client_Cfg.Port);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Master     : %d", MQTT_Client_Cfg.Master);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Slave      : %d", MQTT_Client_Cfg.Slave);
  F_LOGW(true, true, LC_GREY, "MQTT_Client Keep_Alive : %d", MQTT_Client_Cfg.Keep_Alive);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static Configuration_Item_t mqttItem;
Configuration_Item_t *init_mqtt_config (void)
{
  mqttItem.config_keywords  = MqttConfig_Keywords;
  mqttItem.num_keywords     = sizeof (MqttConfig_Keywords) / sizeof (Config_Keyword_t);
  mqttItem.defaults         = mqtt_settings;
  mqttItem.get_config       = mqtt_get_config;
  mqttItem.compare_config   = mqtt_compare_config;
  mqttItem.update_config    = mqtt_update_config;
  mqttItem.apply_config     = NULL;

  return &mqttItem;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

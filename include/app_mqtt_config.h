#ifndef __MQTT_CONFIG_H__
#define __MQTT_CONFIG_H__

#include "app_configs.h"                // Configuration_Item_t

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define STR_MQTT_CLIENT_NAME      "mqtt_name"
#define STR_MQTT_CLIENT_SERVER    "mqtt_server"
#define STR_MQTT_CLIENT_URI       "mqtt_uri"
#define STR_MQTT_CLIENT_ID        "mqtt_client_id"
#define STR_MQTT_CLIENT_USERNAME  "mqtt_user"
#define STR_MQTT_CLIENT_PASSWORD  "mqtt_password"
#define STR_MQTT_CLIENT_PORT      "mqtt_port"
#define STR_MQTT_CLIENT_MASTER    "mqtt_master"
#define STR_MQTT_CLIENT_SLAVE     "mqtt_slave"
#define STR_MQTT_CLIENT_KEEPALIVE "mqtt_keep_alive"

typedef enum mqtt_devices
{
  // listen devices
  dev_Light,
  dev_LEDs,
  dev_Pattern,
  dev_Connect,
  dev_Discon,
  dev_hikAlarm,
  dev_Network,
  dev_Radar,
  dev_Lux,

  num_devices
} mqtt_devices_t;

typedef enum led_commands
{
  CMD_LED_PWR,
  CMD_LED_COL1,
  CMD_LED_COL2,
  CMD_LED_PAL1,
  CMD_LED_PAL2,
  CMD_LED_DIR1,
  CMD_LED_IDX1,
  CMD_LED_SCOL,
  CMD_LED_LOOP_US,
  CMD_LED_BRIGHTNESS,
  CMD_LED_PARAMS,
} led_commands_t;

#define MQTT_CLIENT_STRLEN_NAME       32
#define MQTT_CLIENT_STRLEN_SERVER     64
#define MQTT_CLIENT_STRLEN_URI        128
#define MQTT_CLIENT_STRLEN_ID         32
#define MQTT_CLIENT_STRLEN_USERNAME   32
#define MQTT_CLIENT_STRLEN_PASSWORD   32

#define MQTT_SERVER_STRLEN_NAME       32
#define MQTT_SERVER_STRLEN_TOPIC_SUB  32
#define MQTT_SERVER_STRLEN_TOPOC_PUB  32

typedef struct
{
  char      Name[MQTT_CLIENT_STRLEN_NAME+1];
  char      Server[MQTT_CLIENT_STRLEN_SERVER+1];
  char      Uri[MQTT_CLIENT_STRLEN_URI+1];
  uint32_t  Port;
  char      Client_ID[MQTT_CLIENT_STRLEN_ID+1];
  char      Username[MQTT_CLIENT_STRLEN_USERNAME+1];
  char      Password[MQTT_CLIENT_STRLEN_PASSWORD+1];
  int       Keep_Alive;
  bool      Master;
  bool      Slave;
} MQTT_client_cfg_t;

typedef struct
{
  char     Name[MQTT_SERVER_STRLEN_NAME];
  char     Topic_sub[MQTT_SERVER_STRLEN_TOPIC_SUB];
  char     Topic_pub[MQTT_SERVER_STRLEN_TOPOC_PUB];
  bool     Enable_publish;
  bool     Retained;
  uint8_t  QoS;
  uint8_t  dmy;
} MQTT_server_cfg_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern MQTT_server_cfg_t MQTT_server_cfg[num_devices];
extern MQTT_client_cfg_t MQTT_Client_Cfg;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Configuration_Item_t *init_mqtt_config (void);

// ***********************************************
int _set_mqtt_setting (char *buf, int bufsize, char *param, char *value, int setting);
int _get_mqtt_setting (char *buf, int bufsize, int setting);

void getMqttConfig (void);
void _get_nvs_mqtt_config (void);


#endif // __MQTT_CONFIG_H__

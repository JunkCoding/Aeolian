#ifndef __MQTT_SETTINGS_H__
#define __MQTT_SETTINGS_H__

#define ICACHE_RODATA_ATTR
#define STORE_ATTR __attribute__( ( aligned( 4 ) ) )

#include "app_configs.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#if defined (fucking_error)
#define  DEV_NAME          0x01
#define  TOPIC_SUB         0x02
#define  TOPIC_PUB         0x03
#define  ENABLE_PUBLISH    0x04
#define  RETAINED          0x05
#define  QOS               0x06
#define  ITEM_FIRST        DEV_NAME
#define  ITEM_LAST         QOS

#define  GRP_LIGHT         0x10
#define  GRP_LEDS          0x20
#define  GRP_PATTERN       0x30
#define  GRP_CONNECT       0x40
#define  GRP_DISCON        0x50
#define  GRP_HIKALARM      0x60
#define  GRP_NETWORK       0x70
#define  GRP_RADAR         0x80
#define  GRP_LUX           0x90
#define  GRP_FIRST         GRP_LIGHT
#define  GRP_LAST          GRP_LUX

typedef enum
{
  MQTT_CLIENT_ID,
  MQTT_URI,
  MQTT_USERNAME,
  MQTT_PASSWORD,
  MQTT_PORT,
  MQTT_KEEP_ALIVE,
  MQTT_MASTER,
  MQTT_SLAVE
} mqtt_type_t;
#define  END_OF_MQTT_LIST  0xFF
#else
enum
{
  DEV_NAME          = 0x01,
  TOPIC_SUB         = 0x02,
  TOPIC_PUB         = 0x03,
  ENABLE_PUBLISH    = 0x04,
  RETAINED          = 0x05,
  QOS               = 0x06,
  ITEM_FIRST        = DEV_NAME,
  ITEM_LAST         = QOS,
  GRP_LIGHT         = 0x10,
  GRP_LEDS          = 0x20,
  GRP_PATTERN       = 0x30,
  GRP_CONNECT       = 0x40,
  GRP_DISCON        = 0x50,
  GRP_HIKALARM      = 0x60,
  GRP_NETWORK       = 0x70,
  GRP_RADAR         = 0x80,
  GRP_LUX           = 0x90,
  GRP_FIRST         = GRP_LIGHT,
  GRP_LAST          = GRP_LUX,
};

typedef enum
{
  MQTT_CLIENT_ID,
  MQTT_URI,
  MQTT_USERNAME,
  MQTT_PASSWORD,
  MQTT_PORT,
  MQTT_KEEPALIVE,
  MQTT_MASTER,
  MQTT_SLAVE
} mqtt_type_t;
#endif
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

extern const_settings_t mqtt_settings[];

void config_print_mqtt_defaults( void );

#endif //  __MQTT_SETTINGS_H__

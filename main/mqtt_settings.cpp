

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------


#include <stdio.h>
#include <string.h>

#include "app_configs.h"
#include "mqtt_settings.h"
#include "app_utils.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define Default_0x11    "Light"                 // Name
#define Default_0x12    "AXRGB/light"           // Topic_sub
#define Default_0x13    "AXRGB/stat/%c/light"   // Topic_pub
#define Default_0x14    "1"                     // Enable_publish
#define Default_0x15    "0"                     // Retained
#define Default_0x16    "0"                     // QoS

#define Default_0x21    "LEDs"                  // Name
#define Default_0x22    "AXRGB/leds"            // Topic_sub
#define Default_0x23    "AXRGB/stat/%c/leds"    // Topic_pub
#define Default_0x24    "1"                     // Enable_publish
#define Default_0x25    "0"                     // Retained
#define Default_0x26    "0"                     // QoS

#define Default_0x31    "Pattern"               // Name
#define Default_0x32    "AXRGB/pattern"         // Topic_sub
#define Default_0x33    "AXRGB/stat/%c/pattern" // Topic_pub
#define Default_0x34    "1"                     // Enable_publish
#define Default_0x35    "1"                     // Retained
#define Default_0x36    "0"                     // QoS

#define Default_0x41    "Connect"               // Name
#define Default_0x42    "AXRGB/clients"         // Topic_sub
#define Default_0x43    "AXRGB/clients/%c"      // Topic_pub
#define Default_0x44    "1"                     // Enable_publish
#define Default_0x45    "1"                     // Retained
#define Default_0x46    "0"                     // QoS

#define Default_0x51    "Disconnect"            // Name
#define Default_0x52    "AXRGB/clients"         // Topic_sub
#define Default_0x53    "AXRGB/clients/%c"      // Topic_pub
#define Default_0x54    "1"                     // Enable_publish
#define Default_0x55    "0"                     // Retained
#define Default_0x56    "0"                     // QoS

#define Default_0x61    "hikAlarm"              // Name
#define Default_0x62    "AXRGB/hikAlarm"        // Topic_sub
#define Default_0x63    "AXRGB/hikctrl/%c"      // Topic_pub
#define Default_0x64    "1"                     // Enable_publish
#define Default_0x65    "0"                     // Retained
#define Default_0x66    "0"                     // QoS

#define Default_0x71    "Network"               // Name
#define Default_0x72    "BTRGB/network"         // Topic_sub
#define Default_0x73    "BTRGB/network/%c"      // Topic_pub
#define Default_0x74    "1"                     // Enable_publish
#define Default_0x75    "0"                     // Retained
#define Default_0x76    "0"                     // QoS

#define Default_0x81    "mmW Radar"             // Name
#define Default_0x82    "BTRGB/presence/mmw"    // Topic_sub
#define Default_0x83    "BTRGB/presence/mmw"    // Topic_pub
#define Default_0x84    "1"                     // Enable_publish
#define Default_0x85    "0"                     // Retained
#define Default_0x86    "0"                     // QoS

#define Default_0x91    "Lux Level"             // Name
#define Default_0x92    "BTRGB/presence/lux"    // Topic_sub
#define Default_0x93    "BTRGB/presence/lux"    // Topic_pub
#define Default_0x94    "1"                     // Enable_publish
#define Default_0x95    "0"                     // Retained
#define Default_0x96    "0"                     // QoS

#define Default_0xFF    ""                      // end of list

// --------------------------------------------------------------------------

const char String_0x11[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x11;  // Name
const char String_0x12[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x12;  // Topic_sub
const char String_0x13[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x13;  // Topic_pub
const char String_0x14[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x14;  // Enable_publish
const char String_0x15[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x15;  // Retained
const char String_0x16[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x16;  // QoS

const char String_0x21[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x21;  // Name
const char String_0x22[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x22;  // Topic_sub
const char String_0x23[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x23;  // Topic_pub
const char String_0x24[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x24;  // Enable_publish
const char String_0x25[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x25;  // Retained
const char String_0x26[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x26;  // QoS

const char String_0x31[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x31;  // Name
const char String_0x32[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x32;  // Topic_sub
const char String_0x33[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x33;  // Topic_pub
const char String_0x34[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x34;  // Enable_publish
const char String_0x35[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x35;  // Retained
const char String_0x36[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x36;  // QoS

const char String_0x41[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x41;  // Name
const char String_0x42[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x42;  // Topic_sub
const char String_0x43[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x43;  // Topic_pub
const char String_0x44[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x44;  // Enable_publish
const char String_0x45[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x45;  // Retained
const char String_0x46[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x46;  // QoS

const char String_0x51[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x51;  // Name
const char String_0x52[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x52;  // Topic_sub
const char String_0x53[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x53;  // Topic_pub
const char String_0x54[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x54;  // Enable_publish
const char String_0x55[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x55;  // Retained
const char String_0x56[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x56;  // QoS

const char String_0x61[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x61;  // Name
const char String_0x62[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x62;  // Topic_sub
const char String_0x63[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x63;  // Topic_pub
const char String_0x64[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x64;  // Enable_publish
const char String_0x65[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x65;  // Retained
const char String_0x66[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x66;  // QoS

const char String_0x71[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x71;  // Name
const char String_0x72[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x72;  // Topic_sub
const char String_0x73[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x73;  // Topic_pub
const char String_0x74[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x74;  // Enable_publish
const char String_0x75[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x75;  // Retained
const char String_0x76[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x76;  // QoS

const char String_0x81[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x81;  // Name
const char String_0x82[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x82;  // Topic_sub
const char String_0x83[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x83;  // Topic_pub
const char String_0x84[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x84;  // Enable_publish
const char String_0x85[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x85;  // Retained
const char String_0x86[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x86;  // QoS

const char String_0x91[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x91;  // Name
const char String_0x92[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x92;  // Topic_sub
const char String_0x93[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x93;  // Topic_pub
const char String_0x94[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x94;  // Enable_publish
const char String_0x95[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x95;  // Retained
const char String_0x96[] ICACHE_RODATA_ATTR STORE_ATTR = Default_0x96;  // QoS

// --------------------------------------------------------------------------
// 0xFF is the value after eraseing the flash, so when the value field is not 0xFF
// the whole record is invalid

const_settings_t mqtt_settings[] ICACHE_RODATA_ATTR STORE_ATTR =
{
   { { { 0x11, TEXT,     sizeof (Default_0x11) - 1, 0xFF } }, String_0x11 }, // Name
   { { { 0x12, TEXT,     sizeof (Default_0x12) - 1, 0xFF } }, String_0x12 }, // Topic_sub
   { { { 0x13, TEXT,     sizeof (Default_0x13) - 1, 0xFF } }, String_0x13 }, // Topic_pub
   { { { 0x14, FLAG,     sizeof (Default_0x14) - 1, 0xFF } }, String_0x14 }, // Enable_publish
   { { { 0x15, FLAG,     sizeof (Default_0x15) - 1, 0xFF } }, String_0x15 }, // Retained
   { { { 0x16, NUMBER,   sizeof (Default_0x16) - 1, 0xFF } }, String_0x16 }, // QoS

   { { { 0x21, TEXT,     sizeof (Default_0x21) - 1, 0xFF } }, String_0x21 }, // Name
   { { { 0x22, TEXT,     sizeof (Default_0x22) - 1, 0xFF } }, String_0x22 }, // Topic_sub
   { { { 0x23, TEXT,     sizeof (Default_0x23) - 1, 0xFF } }, String_0x23 }, // Topic_pub
   { { { 0x24, FLAG,     sizeof (Default_0x24) - 1, 0xFF } }, String_0x24 }, // Enable_publish
   { { { 0x25, FLAG,     sizeof (Default_0x25) - 1, 0xFF } }, String_0x25 }, // Retained
   { { { 0x26, NUMBER,   sizeof (Default_0x26) - 1, 0xFF } }, String_0x26 }, // QoS

   { { { 0x31, TEXT,     sizeof (Default_0x31) - 1, 0xFF } }, String_0x31 }, // Name
   { { { 0x32, TEXT,     sizeof (Default_0x32) - 1, 0xFF } }, String_0x32 }, // Topic_sub
   { { { 0x33, TEXT,     sizeof (Default_0x33) - 1, 0xFF } }, String_0x33 }, // Topic_pub
   { { { 0x34, FLAG,     sizeof (Default_0x34) - 1, 0xFF } }, String_0x34 }, // Enable_publish
   { { { 0x35, FLAG,     sizeof (Default_0x35) - 1, 0xFF } }, String_0x35 }, // Retained
   { { { 0x36, NUMBER,   sizeof (Default_0x36) - 1, 0xFF } }, String_0x36 }, // QoS

   { { { 0x41, TEXT,     sizeof (Default_0x41) - 1, 0xFF } }, String_0x41 }, // Name
   { { { 0x42, TEXT,     sizeof (Default_0x42) - 1, 0xFF } }, String_0x42 }, // Topic_sub
   { { { 0x43, TEXT,     sizeof (Default_0x43) - 1, 0xFF } }, String_0x43 }, // Topic_pub
   { { { 0x44, FLAG,     sizeof (Default_0x44) - 1, 0xFF } }, String_0x44 }, // Enable_publish
   { { { 0x45, FLAG,     sizeof (Default_0x45) - 1, 0xFF } }, String_0x45 }, // Retained
   { { { 0x46, NUMBER,   sizeof (Default_0x46) - 1, 0xFF } }, String_0x46 }, // QoS

   { { { 0x51, TEXT,     sizeof (Default_0x51) - 1, 0xFF } }, String_0x51 }, // Name
   { { { 0x52, TEXT,     sizeof (Default_0x52) - 1, 0xFF } }, String_0x52 }, // Topic_sub
   { { { 0x53, TEXT,     sizeof (Default_0x53) - 1, 0xFF } }, String_0x53 }, // Topic_pub
   { { { 0x54, FLAG,     sizeof (Default_0x54) - 1, 0xFF } }, String_0x54 }, // Enable_publish
   { { { 0x55, FLAG,     sizeof (Default_0x55) - 1, 0xFF } }, String_0x55 }, // Retained
   { { { 0x56, NUMBER,   sizeof (Default_0x56) - 1, 0xFF } }, String_0x56 }, // QoS

   { { { 0x61, TEXT,     sizeof (Default_0x61) - 1, 0xFF } }, String_0x61 }, // Name
   { { { 0x62, TEXT,     sizeof (Default_0x62) - 1, 0xFF } }, String_0x62 }, // Topic_sub
   { { { 0x63, TEXT,     sizeof (Default_0x63) - 1, 0xFF } }, String_0x63 }, // Topic_pub
   { { { 0x64, FLAG,     sizeof (Default_0x64) - 1, 0xFF } }, String_0x64 }, // Enable_publish
   { { { 0x65, FLAG,     sizeof (Default_0x65) - 1, 0xFF } }, String_0x65 }, // Retained
   { { { 0x66, NUMBER,   sizeof (Default_0x66) - 1, 0xFF } }, String_0x66 }, // QoS

   { { { 0x71, TEXT,     sizeof (Default_0x71) - 1, 0xFF } }, String_0x71 }, // Name
   { { { 0x72, TEXT,     sizeof (Default_0x72) - 1, 0xFF } }, String_0x72 }, // Topic_sub
   { { { 0x73, TEXT,     sizeof (Default_0x73) - 1, 0xFF } }, String_0x73 }, // Topic_pub
   { { { 0x74, FLAG,     sizeof (Default_0x74) - 1, 0xFF } }, String_0x74 }, // Enable_publish
   { { { 0x75, FLAG,     sizeof (Default_0x75) - 1, 0xFF } }, String_0x75 }, // Retained
   { { { 0x76, NUMBER,   sizeof (Default_0x76) - 1, 0xFF } }, String_0x76 }, // QoS

   { { { 0x81, TEXT,     sizeof (Default_0x81) - 1, 0xFF } }, String_0x81 }, // Name
   { { { 0x82, TEXT,     sizeof (Default_0x82) - 1, 0xFF } }, String_0x82 }, // Topic_sub
   { { { 0x83, TEXT,     sizeof (Default_0x83) - 1, 0xFF } }, String_0x83 }, // Topic_pub
   { { { 0x84, FLAG,     sizeof (Default_0x84) - 1, 0xFF } }, String_0x84 }, // Enable_publish
   { { { 0x85, FLAG,     sizeof (Default_0x85) - 1, 0xFF } }, String_0x85 }, // Retained
   { { { 0x86, NUMBER,   sizeof (Default_0x86) - 1, 0xFF } }, String_0x86 }, // QoS

   { { { 0x91, TEXT,     sizeof (Default_0x91) - 1, 0xFF } }, String_0x91 }, // Name
   { { { 0x92, TEXT,     sizeof (Default_0x92) - 1, 0xFF } }, String_0x92 }, // Topic_sub
   { { { 0x93, TEXT,     sizeof (Default_0x93) - 1, 0xFF } }, String_0x93 }, // Topic_pub
   { { { 0x94, FLAG,     sizeof (Default_0x94) - 1, 0xFF } }, String_0x94 }, // Enable_publish
   { { { 0x95, FLAG,     sizeof (Default_0x95) - 1, 0xFF } }, String_0x95 }, // Retained
   { { { 0x96, NUMBER,   sizeof (Default_0x96) - 1, 0xFF } }, String_0x96 }, // QoS

   { { { 0xFF, 0xFF, 0xFF, 0xFF } },  NULL }   // end of list
};

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void  config_print_mqtt_defaults (void)
{
  F_LOGV(true, true, LC_BRIGHT_GREEN, "begin");

  int i = 0;
  char default_str[64 + 1] __attribute__ ((aligned (4)));  // on extra byte for terminating null character
  cfg_mode_t mode __attribute__ ((aligned (4)));

  while ( true )
  {
    if ( ((uint32_t)(&mqtt_settings[i].mode) & 3) != 0 )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
    }
    else if ( ((uint32_t)(&mode) & 3) != 0 )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
    }
    else
    {
      memcpy (&mode, &mqtt_settings[i].id, sizeof (mode));
      if ( mode.id == 0xff )  // end of list
        break;

      if ( mode.len > sizeof (default_str) - 1 )
        mode.len = sizeof (default_str) - 1;

      int len4 = (mode.len + 3) & ~3;
      if ( len4 > sizeof (default_str) )
      {
        len4 = sizeof (default_str) & ~3;
        mode.len = len4 - 1;
      }

      if ( ((uint32_t)mqtt_settings[i].text & 3) != 0 )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
      }
      else if ( ((uint32_t)default_str & 3) != 0 )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
      }
      else
      {
        memcpy (default_str, mqtt_settings[i].text, len4);
        default_str[mode.len] = 0; // terminate string
        //printf("id 0x%02x: %1d %3d \"%s\" %d\r\n", mode.id, mode.type, mode.len, default_str, len4);
      }
    }
    i++;
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

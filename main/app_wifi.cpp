

// **************************************************************************************************
//
// **************************************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_system.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <esp_wifi_types.h>
#include <esp_chip_info.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include <jsmn.h>

#include "app_main.h"
#include "app_utils.h"
#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_httpd.h"
#include "app_mqtt_settings.h"
#include "app_mqtt_config.h"
#include "app_configs.h"
#include "app_sntp.h"
#include "app_schedule.h"
#include "app_flash.h"
#include "app_yuarel.h"


// **************************************************************************************************
//
// **************************************************************************************************
#define UNIQ_NAME_PREFIX  "lights"
#define MAC_STR           "%02x%02x%02x%02x%02x%02x"
#define UNIQ_NAME_LEN     (sizeof(UNIQ_NAME_PREFIX) + (6 * 2)) /* 6 bytes for mac address as 2 hex chars */

#define SCAN_TIMEOUT      (60 * 1000 / portTICK_PERIOD_MS)
#define CFG_TIMEOUT       (60 * 1000 / portTICK_PERIOD_MS)
#define CFG_TICKS         (1000 / portTICK_PERIOD_MS)
#define CFG_DELAY         (100 / portTICK_PERIOD_MS)

// **************************************************************************************************
// global variables
// **************************************************************************************************
wifi_sta_cfg_t    wifi_sta_cfg      = {};
wifi_ap_cfg_t     wifi_ap_cfg       = {};
wifi_cfg_t        wifi_cfg          = {};

size_t            hostname_len      = 0;
char hostname[64] __attribute__ ((aligned (4))) = {};

static httpd_handle_t *hHttpServer  = NULL;
static esp_netif_t *netif_sta       = NULL;
static esp_netif_t *netif_ap        = NULL;
esp_timer_handle_t xHandleCheckSTA  = NULL;
static char ipAddr[16];

static EventGroupHandle_t wifi_event_group = NULL;

const int WIFI_INITIALIZED          = BIT0;
const int WIFI_STA_CONNECTED        = BIT1;  // ESP32 is currently connected
const int WIFI_AP_STACONNECTED      = BIT2;
const int WIFI_STA_STARTED          = BIT4;
const int WIFI_AP_STARTED           = BIT5;  // Set automatically once the SoftAP is started
const int WIFI_SCAN_INPROGRESS      = BIT6;  // Set when scan called, cleared when complete
const int WIFI_SCAN_DONE            = BIT7;  // Cleared when called, set when done
const int WIFI_TEST_STA_CFG         = BIT8;  // Testing STA configuration


// **************************************************************************************************
// Scan result
// **************************************************************************************************
static QueueHandle_t xApScanQueue = NULL;

// **************************************************************************************************
// Function declarations
// **************************************************************************************************
static void start_ap(void);
static void stop_ap (bool forceStop=false);
static void periodic_sta_callback(void *arg);
static bool sta_connect (wifi_sta_cfg_t *sta_cfg);

// **************************************************************************************************
//
// **************************************************************************************************

/* Strength of authmodes */
/* OPEN < WEP < WPA_PSK < OWE < WPA2_PSK = WPA_WPA2_PSK < WAPI_PSK < WPA3_PSK = WPA2_WPA3_PSK = DPP */
const char *auth2str (int auth)
{
  switch ( auth )
  {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2 Enterprise";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI";
    case WIFI_AUTH_OWE:
      return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:
      return "WPA2 ENT 192";
    case WIFI_AUTH_WPA3_EXT_PSK:
      return "WPA3 EXT";
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE:
      return "WPA3 EXT MIXED MODE";
    case WIFI_AUTH_DPP:
      return "DPP";
    default:
      return "Unknown";
  }
}

const char *cypher2str (int cypher)
{
  switch ( cypher )
  {
    case WIFI_CIPHER_TYPE_NONE:
      return "None";
    case WIFI_CIPHER_TYPE_WEP40:
      return "WEP40";
    case WIFI_CIPHER_TYPE_WEP104:
      return "WEP104";
    case WIFI_CIPHER_TYPE_TKIP:
      return "TKIP";
    case WIFI_CIPHER_TYPE_CCMP:
      return "CCMP";
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      return "TKIP CCMP";
    case WIFI_CIPHER_TYPE_AES_CMAC128:
      return "AES CMAC128";
    case WIFI_CIPHER_TYPE_SMS4:
      return "SMS4";
    case WIFI_CIPHER_TYPE_GCMP:
      return "GCMP";
    case WIFI_CIPHER_TYPE_GCMP256:
      return "GCMP256";
    case WIFI_CIPHER_TYPE_AES_GMAC128:
      return "AES GMAC128";
    case WIFI_CIPHER_TYPE_AES_GMAC256:
      return "AES GMAC256";
    default:
      return "Unknown";
  }
}
const char *mode2str (int mode)
{
  switch ( mode )
  {
    case WIFI_MODE_NULL:
      return "Disabled";
    case WIFI_MODE_STA:
      return "Station";
    case WIFI_MODE_AP:
      return "SoftAP";
    case WIFI_MODE_APSTA:
      return "Station + SoftAP";
    default:
      return "Unknown";
  }
}
const char *SecondChanStr (wifi_second_chan_t second)
{
  switch ( second )
  {
    case WIFI_SECOND_CHAN_NONE:
      return (char *)"";
    case WIFI_SECOND_CHAN_ABOVE:
      return (char *)"+";
    case WIFI_SECOND_CHAN_BELOW:
      return (char *)"-";
    default:
      return "Unknown";
  }
}
const char *mac2str (unsigned char *mac_bin)
{
  static char mac_str_buf[18];
  snprintf (mac_str_buf, sizeof (mac_str_buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac_bin[0], mac_bin[1], mac_bin[2], mac_bin[3], mac_bin[4], mac_bin[5]);
  return (const char *)mac_str_buf;
}
bool str2mac (const char *mac, uint8_t *values)
{
  sscanf (mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
  return strlen (mac) == 17;
}

void set_hostname (const char* hostname)
{
  if ( netif_ap )
  {
    esp_netif_set_hostname(netif_ap, hostname);
  }

  if ( netif_sta )
  {
    esp_netif_set_hostname(netif_sta, hostname);
  }
}

// **************************************************************************************************
// Callback to check STA connected to AP correctly
// **************************************************************************************************
static void periodic_sta_callback(void *arg)
{
  // Prevent multiple instances/spam
  if ( (char *)arg == NULL && xHandleCheckSTA != NULL )
  {
    F_LOGW(true, true, LC_YELLOW, "periodic_sta_callback() already running!");
    return;
  }

  // Clear the handle
  xHandleCheckSTA = NULL;

  // Check if we need to run again
  EventBits_t uxBits = xEventGroupGetBits (wifi_event_group);
  if ( !BTST(uxBits, WIFI_STA_CONNECTED) )
  {
    esp_wifi_connect();

#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
    // Create a callback function to check if we were able to connect to an AP.
    // If we fail to connect to an AP we will start an AP so we can be connected
    // to and configured.
    const esp_timer_create_args_t check_sta_args = {
      .callback = &periodic_sta_callback,
      .arg      = &xHandleCheckSTA,
      .name     = "Check STA"};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

    if ( esp_timer_create (&check_sta_args, &xHandleCheckSTA) == ESP_OK )
    {
      esp_timer_start_once(xHandleCheckSTA, ((prng() % 60) + 5) * 1000 * 1000);
    }
  }
}
// **************************************************************************************************
// b
// **************************************************************************************************
void wifi_eventHandler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  static uint16_t disconnects = 0;

  F_LOGV(true, true, LC_BRIGHT_MAGENTA, "WiFi Event ID: %d", event_id);

  switch ( event_id )
  {
    case WIFI_EVENT_WIFI_READY:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_WIFI_READY");
        break;
      }
    case WIFI_EVENT_SCAN_DONE:
      {
        F_LOGV(true, true, LC_GREY, "WIFI_EVENT_SCAN_DONE");

        // We can set this one here, at the beginning
        xEventGroupSetBits (wifi_event_group, WIFI_SCAN_DONE);

        // No point processing if we don't have a queue
        if ( xApScanQueue != NULL )
        {
          scan_result_t cgiWifiAps = {0, 0};

          // How many AP's did we find?
          esp_wifi_scan_get_ap_num(&cgiWifiAps.apCount);
          F_LOGV(true, true, LC_GREY, "Scan done: found %d APs", cgiWifiAps.apCount);

          if ( cgiWifiAps.apCount > 0 )
          {
            cgiWifiAps.apList = (wifi_ap_record_t *)pvPortMalloc(sizeof(wifi_ap_record_t) * cgiWifiAps.apCount);
            esp_wifi_scan_get_ap_records(&cgiWifiAps.apCount, cgiWifiAps.apList);

            // Is the queue available?
            if ( xQueueSendToBack(xApScanQueue, (void *)&cgiWifiAps, 0) != pdPASS )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "xQueueSendToBack failed to queue new item");
            }
          }
        }

        // And clear this one when the data has been processed
        xEventGroupClearBits (wifi_event_group, WIFI_SCAN_INPROGRESS);
        break;
      }
      // **************************************************************************************************
      // STATION EVENTS
      // **************************************************************************************************
    case WIFI_EVENT_STA_START:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_STA_START");
        xEventGroupSetBits (wifi_event_group, WIFI_STA_STARTED);

        const char *hostName;
        esp_netif_get_hostname(netif_sta, &hostName);
        F_LOGI(true, true, LC_YELLOW, "STA hostname: \"%s\"", hostName);

        // Fin
        break;
      }
    case WIFI_EVENT_STA_STOP:
      {
        F_LOGW(true, true, LC_YELLOW, "WIFI_EVENT_STA_STOP");
        xEventGroupClearBits (wifi_event_group, WIFI_STA_STARTED);

        // Fin
        break;
      }
    case WIFI_EVENT_STA_CONNECTED:
      {
        F_LOGI(true, true, LC_GREEN, "WIFI_EVENT_STA_CONNECTED");
        xEventGroupSetBits (wifi_event_group, WIFI_STA_CONNECTED);

        // Check if we need to start the webserver
        if ( *hHttpServer == NULL )
        {
          *hHttpServer = start_webserver ();
        }

        // Check if we need to stop our SoftAP
        stop_ap();

        // Fin
        break;
      }
    case WIFI_EVENT_STA_DISCONNECTED:
      {

        F_LOGW(true, true, LC_RED, "WIFI_EVENT_STA_DISCONNECTED");

        EventBits_t uxBits = xEventGroupGetBits (wifi_event_group);
        if ( BTST (uxBits, WIFI_STA_CONNECTED) )
        {
          disconnects = 0;
          xEventGroupClearBits (wifi_event_group, WIFI_STA_CONNECTED);
        }

        /* Are we testing an STA config? */
        if ( BTST (uxBits, WIFI_TEST_STA_CFG) )
        {
          F_LOGW (true, true, LC_GREY, "Ignoring disconnect whilst testing STA config");
          break;
        }
        // If we have more than 5 disconnects, start an AP to allow configuration
        else if ( ++disconnects >= 5 )
        {
          wifi_mode_t  cMode;
          esp_wifi_get_mode(&cMode);
          if ( !(cMode & WIFI_MODE_AP) )
          {
            F_LOGE(true, true, LC_YELLOW, "Too many disconnects, starting AP");

            // Too many disconnects, start AP
            start_ap();
          }
        }
        else
        {
          periodic_sta_callback(NULL);
        }

        // Check if we need to start the webserver
        // FixMe: Currently crashes in app_websockets.cpp:442
        //check_stopHttpServer();

        // Fin
        break;
      }
    case WIFI_EVENT_STA_AUTHMODE_CHANGE:
      {
        F_LOGW(true, true, LC_YELLOW, "WIFI_EVENT_STA_AUTHMODE_CHANGE");

        // Fin
        break;
      }
      // **************************************************************************************************
      // ACCESS POINT EVENTS
      // **************************************************************************************************
    case WIFI_EVENT_AP_START:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_AP_START");
        xEventGroupSetBits (wifi_event_group, WIFI_AP_STARTED);

        // Check if we need to start the webserver
        if ( *hHttpServer == NULL )
        {
          *hHttpServer = start_webserver();
        }

        // Fin
        break;
      }
    case WIFI_EVENT_AP_STOP:
      {
        F_LOGW(true, true, LC_RED, "WIFI_EVENT_AP_STOP");
        xEventGroupClearBits (wifi_event_group, WIFI_AP_STARTED);

        // Check if we need to start the webserver
        // FixMe: When enabled, we get core dumps in websocket server
        //check_stopHttpServer();

        // Fin
        break;
      }
    case WIFI_EVENT_AP_STACONNECTED:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_AP_STACONNECTED");

        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        F_LOGI(true, true, LC_GREY, "station " MACSTR " join, AID=%d", MAC2STR (event->mac), event->aid);

        // Fin
        break;
      }
    case WIFI_EVENT_AP_STADISCONNECTED:
      {
        F_LOGW(true, true, LC_YELLOW, "WIFI_EVENT_AP_STADISCONNECTED");
        // Fin
        break;
      }
    case WIFI_EVENT_AP_PROBEREQRECVED:
      {
        F_LOGI(true, true, LC_YELLOW, "WIFI_EVENT_AP_PROBEREQRECVED");
        // Fin
        break;
      }
      // **************************************************************************************************
      // MISC
      // **************************************************************************************************
    case WIFI_EVENT_FTM_REPORT:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_FTM_REPORT");
        // Fin
        break;
      }
    case WIFI_EVENT_STA_BSS_RSSI_LOW:
      {
        F_LOGW(true, true, LC_GREY, "WIFI_EVENT_STA_BSS_RSSI_LOW");
        // Fin
        break;
      }
    case WIFI_EVENT_ACTION_TX_STATUS:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_ACTION_TX_STATUS");
        // Fin
        break;
      }
    case WIFI_EVENT_ROC_DONE:
      {
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_ROC_DONE");
        // Fin
        break;
      }
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
      {
        F_LOGW(true, true, LC_GREY, "WIFI_EVENT_STA_BEACON_TIMEOUT");
        // Fin
        break;
      }
    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
      {
        wifi_event_home_channel_change_t *chan_data = (wifi_event_home_channel_change_t *) event_data;
        F_LOGI(true, true, LC_GREY, "WIFI_EVENT_HOME_CHANNEL_CHANGE: channel changed from %u to %u.", chan_data->old_chan, chan_data->new_chan);
        // Fin
        break;
      } 
    default:
      F_LOGE(true, true, LC_YELLOW, "Unhandled WiFi Event ID: %d", event_id);
      // Fin
      break;
  }
}

// **************************************************************************************************
//
// **************************************************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void ip_eventHandler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
#else
void ip_eventHandler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
#endif
{
  switch ( event_id )
  {
    case IP_EVENT_STA_GOT_IP:
      {
        F_LOGI(true, true, LC_GREEN, "IP_EVENT_STA_GOT_IP");

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sprintf(ipAddr, IPSTR, IP2STR (&event->ip_info.ip));
        F_LOGI(true, true, LC_YELLOW, "Allocated DHCP address: %s", ipAddr);

        xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED);

        obtain_time();

        // Fin
        break;
      }
    case IP_EVENT_STA_LOST_IP:
      {
        F_LOGW(true, true, LC_RED, "WIFI_EVENT_STA_LOST_IP");
        xEventGroupClearBits (wifi_event_group, WIFI_STA_CONNECTED);

        // Fin
        break;
      }
    case IP_EVENT_AP_STAIPASSIGNED:
      {
        F_LOGW(true, true, LC_GREEN, "IP_EVENT_AP_STAIPASSIGNED");

        ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
        sprintf(ipAddr, IPSTR, IP2STR (&event->ip));
        F_LOGW(true, true, LC_GREEN, "STA assigned IP address %s", ipAddr);

        // Fin
        break;
      }
    case IP_EVENT_GOT_IP6:
      {
        F_LOGI(true, true, LC_GREY, "IP_EVENT_GOT_IP6");

        // Fin
        break;
      }
    case IP_EVENT_ETH_GOT_IP:
      {
        F_LOGI(true, true, LC_GREY, "IP_EVENT_ETH_GOT_IP");

        // Fin
        break;
      }
    case IP_EVENT_ETH_LOST_IP:
      {
        F_LOGI(true, true, LC_GREY, "IP_EVENT_ETH_LOST_IP");

        // Fin
        break;
      }
    default:
      F_LOGE(true, true, LC_YELLOW, "Unhandled IP Event ID: %d", event_id);

      // Fin
      break;
  }
}

// **************************************************************************************************
//
// **************************************************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void wifi_startScan (void)
#else
void wifi_startScan (void)
#endif
{
  EventBits_t uxBits = xEventGroupGetBits (wifi_event_group);
  if ( BTST(uxBits, WIFI_SCAN_INPROGRESS) )
  { 
    F_LOGV(true, true, LC_YELLOW, "Scan already in progress...");
  }
  else
  {
    wifi_mode_t  cMode;
    esp_wifi_get_mode(&cMode);

    if ( !(cMode & WIFI_MODE_STA) )
    {
      F_LOGI(true, true, LC_YELLOW, "Enabling 'WIFI_MODE_STA' to allow wireless scanning");
      wifi_set_mode((wifi_mode_t)(cMode | WIFI_MODE_STA));
    }

    esp_wifi_get_mode(&cMode);
    if ( cMode & WIFI_MODE_STA )
    {
      wifi_config_t wifi_config = {};
      if ( ESP_OK == esp_wifi_get_config (WIFI_IF_STA, &wifi_config) )
      {
        wifi_config.sta.bssid_set          = NULL;
        wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
        //wifi_config.sta.scan_method        = WIFI_FAST_SCAN;
        wifi_config.sta.sort_method        = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.threshold.rssi     = -127;
        //wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        // Debug
        // wifiStationConfigDump(&wifi_config);

        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
      }

#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
      // configure and run the scan process in nonblocking mode
      // WIFI_SCAN_TYPE_PASSIVE or WIFI_SCAN_TYPE_ACTIVE
      wifi_scan_config_t scan_config = {
        .ssid                        = NULL,
        .bssid                       = NULL,
        .channel                     = 0,
        .show_hidden                 = true,
        .scan_type                   = WIFI_SCAN_TYPE_PASSIVE, //WIFI_SCAN_TYPE_ACTIVE,
        .scan_time                   = {.active = {.min = 0, .max = 100 }, .passive = 100}
      };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

      // Flag scan in progress
      xEventGroupSetBits (wifi_event_group, WIFI_SCAN_INPROGRESS);

      // Clear scan complete flag
      xEventGroupClearBits (wifi_event_group, WIFI_SCAN_DONE);

      // Non blocking scan
      esp_wifi_scan_start(&scan_config, false);

      F_LOGV(true, true, LC_BRIGHT_YELLOW, "WiFi scan started");
    }
    else
    {
      F_LOGE(true, true, LC_RED, "Cannot start a new scan because not in a station mode");
    }
  }
}

// **************************************************************************************************
//
// **************************************************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t wifi_getApScanResult (scan_result_t *cgiWifiAps)
#else
esp_err_t wifi_getApScanResult (scan_result_t *cgiWifiAps)
#endif
{
  esp_err_t err = ESP_FAIL;

  if ( xApScanQueue != NULL )
  {
    err = xQueueReceive(xApScanQueue, cgiWifiAps, 0);
  }

  return err;
}

// **************************************************************************************************
// Create a unique station name
// **************************************************************************************************
static void getUniqName (char *name)
{
  uint8_t mac[6];
  esp_read_mac (mac, ESP_MAC_WIFI_STA);
  sprintf(name, UNIQ_NAME_PREFIX MAC_STR, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// **************************************************************************************************
// Check AP SSID/Password parameters
// **************************************************************************************************
void verify_ap_config (void)
{
  // Check we have a SSID
  if ( wifi_ap_cfg.ssid_len == 0 || (wifi_ap_cfg.pass_len > 0 && wifi_ap_cfg.pass_len < 8) )
  {
    getUniqName (wifi_ap_cfg.ssid);
    wifi_ap_cfg.ssid_len = strlen (wifi_ap_cfg.ssid);

    strcpy ((char *)wifi_ap_cfg.password, wifi_ap_cfg.ssid);
    wifi_ap_cfg.pass_len       = wifi_ap_cfg.ssid_len;
    wifi_ap_cfg.primary        = 1;
    wifi_ap_cfg.authmode       = WIFI_AUTH_WPA2_PSK;
    wifi_ap_cfg.cypher         = WIFI_CIPHER_TYPE_GCMP256;
    wifi_ap_cfg.max_connection = 2;
  }
  else
  {
    if ( wifi_ap_cfg.primary < 1 || wifi_ap_cfg.primary > 6 )
    {
      wifi_ap_cfg.primary = 1;
    }

    if ( wifi_ap_cfg.bandwidth < WIFI_BW_HT20 || wifi_ap_cfg.bandwidth > WIFI_BW_HT40 )
    {
      wifi_ap_cfg.bandwidth = WIFI_BW_HT20;
    }

    if ( wifi_ap_cfg.authmode >= WIFI_AUTH_MAX )
    {
      wifi_ap_cfg.authmode = WIFI_AUTH_WPA2_PSK;
    }

    if ( wifi_ap_cfg.cypher >= WIFI_CIPHER_TYPE_UNKNOWN )
    {
      wifi_ap_cfg.cypher = WIFI_CIPHER_TYPE_TKIP;
    }

    if ( wifi_ap_cfg.max_connection < 4 )
    {
      wifi_ap_cfg.max_connection = 4;
    }
  }
}

// **************************************************************************************************
// Stop AP mode
// **************************************************************************************************
static void stop_ap (bool forceStop)
{
  // Check if we are not configured to run SoftAP
  if ( !(wifi_cfg.mode & WIFI_MODE_AP) || forceStop )
  {
    wifi_mode_t  cMode;
    esp_wifi_get_mode(&cMode);

    if ( cMode | WIFI_MODE_AP )
    {
      wifi_set_mode((wifi_mode_t)(cMode &~ WIFI_MODE_AP));
    }
  }
}

// **************************************************************************************************
// Start AP mode
// **************************************************************************************************
static void start_ap (void)
{
  F_LOGI(true, true, LC_GREY, "Configuring AP...");

  /*
  // Recommended by Espressif not to use
  wifi_country_t country = {
    .cc = "GB",
    .schan = 1,
    .nchan = 13,
    .max_tx_power = 127,
    .policy = WIFI_COUNTRY_POLICY_AUTO,
  };
  esp_wifi_set_country(&country);
  */

  // Check for an SSID and that the password length is valid
  verify_ap_config();

  F_LOGI(true, true, LC_GREY, "       wifi_ap_cfg.primary = %d", wifi_ap_cfg.primary);
  F_LOGI(true, true, LC_GREY, "      wifi_ap_cfg.authmode = %d", wifi_ap_cfg.authmode);
  F_LOGI(true, true, LC_GREY, "        wifi_ap_cfg.cypher = %d", wifi_ap_cfg.cypher);
  F_LOGI(true, true, LC_GREY, "      wifi_ap_cfg.ssid_len = %d", wifi_ap_cfg.ssid_len);
  F_LOGI(true, true, LC_GREY, "wifi_ap_cfg.max_connection = %d", wifi_ap_cfg.max_connection);

  // Initialise and set some defaults
  wifi_config_t wifi_config = {};
  wifi_config.ap.channel         = wifi_ap_cfg.primary;
  wifi_config.ap.authmode        = (wifi_auth_mode_t)wifi_ap_cfg.authmode;
  wifi_config.ap.pairwise_cipher = (wifi_cipher_type_t)wifi_ap_cfg.cypher;
  wifi_config.ap.max_connection  = wifi_ap_cfg.max_connection;
  wifi_config.ap.ftm_responder   = true;
  wifi_config.ap.beacon_interval = 400;

  // Set the SSID/Password
  if ( wifi_ap_cfg.ssid_len > 0 && wifi_ap_cfg.pass_len > 0 )
  {
    memcpy ((char *)wifi_config.ap.ssid, wifi_ap_cfg.ssid, wifi_ap_cfg.ssid_len);
    memcpy ((char *)wifi_config.ap.password, wifi_ap_cfg.password, wifi_ap_cfg.pass_len);
    wifi_config.ap.ssid_len = wifi_ap_cfg.ssid_len;
    wifi_config.ap.ssid[wifi_ap_cfg.ssid_len] = 0x0;
    wifi_config.ap.password[wifi_ap_cfg.pass_len] = 0x0;
  }

  // Let any watcher know the configuration
  //F_LOGW(true, true, LC_GREEN, "AP: ssid = '%s' (%d), pass = '%s'", wifi_config.ap.ssid, wifi_config.ap.ssid_len, wifi_config.ap.password);
  F_LOGW(true, true, LC_GREEN, "AP: ssid = '%s' (%d), pass = '********'", wifi_config.ap.ssid, wifi_config.ap.ssid_len);

  wifi_mode_t cMode;
  esp_wifi_get_mode(&cMode);
  F_LOGI(true, true, LC_MAGENTA, "WiFi current mode: %s", mode2str(cMode));

  wifi_set_mode((wifi_mode_t)(cMode | WIFI_MODE_AP));
  esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
}

// --------------------------------------------------------------------------
// Local helper function
// --------------------------------------------------------------------------
void log_ap(const wifi_ap_record_t ap, const log_colour_t logCol)
{
  F_LOGI(true, true, logCol, "Found AP: %s (%02X:%02X:%02X:%02X:%02X:%02X), channel: %d, rssi: %d, auth: %s",
        ap.ssid, ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5],
        ap.primary, ap.rssi, auth2str(ap.authmode));
}

// --------------------------------------------------------------------------
// Scan local access points for an AP matching our configuration
// --------------------------------------------------------------------------
bool findBestAP (wifi_sta_cfg_t *searchConfig)
{
  // Prepare everything for our search
  int x = 10;             // Max loops
  int rssi = -2000;       // Impossibly high(low?) value
  bool foundAP = false;   // Did we find a match?

  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  ESP_ERROR_CHECK(esp_task_wdt_status(NULL));

  // Loop until we time out or find a match
  do
  {
    esp_task_wdt_reset();
    EventBits_t uxBits = xEventGroupGetBits(wifi_event_group);
    if ( !BTST(uxBits, WIFI_SCAN_INPROGRESS) )
    {
      scan_result_t cgiWifiAps = {0, 0};

      wifi_getApScanResult(&cgiWifiAps);

      if ( cgiWifiAps.apCount > 0 )
      {
        for ( int i = 0; i < cgiWifiAps.apCount; i++ )
        {
          // Compare this AP with the SSID we are looking for
          if ( strncmp (searchConfig->ssid, ( const char * )cgiWifiAps.apList[i].ssid, searchConfig->ssid_len) == 0 )
          {
            if ( searchConfig->bssid_set )
            {
              if ( !memcmp (searchConfig->bssid, cgiWifiAps.apList[i].bssid, BSSID_BYTELEN) )
              {
                log_ap (cgiWifiAps.apList[i], LC_GREEN);
                foundAP = true;
              }
            }
            // If the signal strength is lower than our current saved, make this our chosen AP
            else if ( cgiWifiAps.apList[i].rssi > rssi )
            {
              // Let anyone watching know what we are doing
              log_ap(cgiWifiAps.apList[i], LC_GREEN);

              // Flag we found something
              foundAP = true;

              // Save the rssi value for later comparison
              rssi = cgiWifiAps.apList[i].rssi;
            }
            else
            {
              // Let anyone watching know what we are doing
              log_ap(cgiWifiAps.apList[i], LC_YELLOW);
            }
          }
          else
          {
            // Let anyone watching know what we are doing
            log_ap(cgiWifiAps.apList[i], LC_GREY);
          }
        }
      }
      // Release memory
      if ( cgiWifiAps.apList != NULL )
      {
        vPortFree(cgiWifiAps.apList);
        cgiWifiAps.apList = NULL;
      }

      // Start a new scan
      if ( !foundAP )
      {
        wifi_startScan();
      }
    }
    else
    {
      // have a break
      delay_ms(2000);
    }
    // Decrement out timeout
    x--;
  } while ( x > 0 && !foundAP );

  if ( x == 0 && !foundAP )
  {
    F_LOGE (true, true, LC_RED, "Could not find AP matching \"%.*s\"", searchConfig->ssid_len, searchConfig->ssid);
  }

  ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));

  F_LOGD(true, true, LC_RED, "x = %d, foundAP = %d", x, foundAP);
  return foundAP;
}

// **************************************************************************************************
// Search for AP and start STA connection
// **************************************************************************************************
static bool sta_connect (wifi_sta_cfg_t *sta_cfg)
{
  bool scan_found = false;

  F_LOGI (true, true, LC_GREY, "Configuring STA...");

  wifi_config_t wifi_config = {};
  esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  wifi_config.sta.channel            = 0;
  wifi_config.sta.listen_interval    = 0;
  wifi_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
  wifi_config.sta.sort_method        = WIFI_CONNECT_AP_BY_SIGNAL;
  wifi_config.sta.threshold.rssi     = -90;
  wifi_config.sta.pmf_cfg.capable    = true;
  //wifi_config.sta.pmf_cfg.required   = false;

  // Whether to set the MAC address of target AP or not.
  // Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP
  // (From testing, the ESP32 automatically chooses the AP with the best signal. I would tend to only enable this feature for
  // security reasons, otherwise this might hinder roaming)
  if ( sta_cfg->bssid_set )
  {
    wifi_config.sta.bssid_set = true;
    memcpy((char*)wifi_config.sta.bssid, sta_cfg->bssid, BSSID_BYTELEN);
  }
  //wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
  //wifi_config.sta.rm_enabled         = true;

  // Set the SSID/Password
  if ( sta_cfg->ssid_len > 0 && sta_cfg->pass_len > 0 )
  {
    // Copy the stored SSID to our configuration
    memcpy ((char *)wifi_config.sta.ssid, sta_cfg->ssid, sta_cfg->ssid_len);
    wifi_config.sta.ssid[sta_cfg->ssid_len] = 0x0;

    // Copy the stored password to our configuration
    memcpy ((char *)wifi_config.sta.password, sta_cfg->password, sta_cfg->pass_len);
    wifi_config.sta.password[sta_cfg->pass_len] = 0x0;

    // Check results for an SSID matching our saved SSID/password combination
    if ( ( scan_found = findBestAP (sta_cfg) ) == true )
    {
      // Attempt a connection to the found SSID
      //F_LOGI(true, true, LC_YELLOW, "Attenpting connection to: '%s', password: '%s'", wifi_config.sta.ssid, wifi_config.sta.password);
      F_LOGI(true, true, LC_YELLOW, "Attenpting connection to: '%s', password: '%s'", wifi_config.sta.ssid, "*");
      ESP_ERROR_CHECK (esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      esp_wifi_connect();
    }
  }

  return scan_found;
}

// **************************************************************************************************
// Check for an existing configuration for STA mode.
// If necessary, set or sanitize existing parameters.
// **************************************************************************************************
void get_nvs_sta_cfg (wifi_sta_cfg_t *wifi_sta_cfg)
{
  const char *default_sta_ssid = WIFI_SSID;
  const char *default_sta_pass = WIFI_PASS;
  nvs_handle_t nvs_handle;

// For that time I locked myself out of the device and didn't want to do a reset...
#if not defined (CONFIG_FORCE_STA_CONFIG)
  esp_err_t err = nvs_open(NVS_WIFI_STA_CFG, NVS_READONLY, &nvs_handle);
  if ( err == ESP_OK )
  {
    wifi_sta_cfg->ssid_len = SSID_STRLEN;
    if ( nvs_get_str(nvs_handle, STR_STA_SSID, wifi_sta_cfg->ssid, &wifi_sta_cfg->ssid_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      wifi_sta_cfg->ssid_len = 0;
    }

    wifi_sta_cfg->pass_len = PASSW_STRLEN;
    if ( nvs_get_str(nvs_handle, STR_STA_PASSW, wifi_sta_cfg->password, &wifi_sta_cfg->pass_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      wifi_sta_cfg->pass_len = 0;
    }

    /*
    wifi_sta_cfg->uname_len = UNAME_STRLEN;
    if ( nvs_get_str (nvs_handle, STR_STA_UNAME, wifi_sta_cfg->username, &wifi_sta_cfg->uname_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      wifi_sta_cfg->uname_len = 0;
    }*/
    nvs_close(nvs_handle);
  }
  else
  {
    F_LOGE(true, true, LC_YELLOW, "Couldn't open flash for \"%s\" (func:%s, line: %d)", NVS_WIFI_STA_CFG,  __FILE__, __LINE__);
  }
#endif /* CONFIG_FORCE_STA_CONFIG */

  // Check for zero config
  if ( !wifi_sta_cfg->ssid_len && !wifi_sta_cfg->pass_len )
  {
    // If we have hardcoded defaults for this eventuality, use them
    wifi_sta_cfg->ssid_len = sprintf(wifi_sta_cfg->ssid, "%s", default_sta_ssid);
    wifi_sta_cfg->pass_len = sprintf(wifi_sta_cfg->password, "%s", default_sta_pass);
  }
}

// **************************************************************************************************
// Check for an existing configuration for AP mode.
// If necessary, set or sanitize existing parameters.
// **************************************************************************************************
void get_nvs_ap_cfg (wifi_ap_cfg_t *wifi_ap_cfg)
{
  nvs_handle_t nvs_handle;

  esp_err_t err = nvs_open(NVS_WIFI_AP_CFG, NVS_READONLY, &nvs_handle);
  if ( err == ESP_OK )
  {
    wifi_ap_cfg->ssid_len = SSID_STRLEN;
    if ( nvs_get_str(nvs_handle, STR_AP_SSID, wifi_ap_cfg->ssid, &wifi_ap_cfg->ssid_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      wifi_ap_cfg->ssid_len = 0;
    }

    wifi_ap_cfg->pass_len = PASSW_STRLEN;
    if ( nvs_get_str(nvs_handle, STR_AP_PASSW, wifi_ap_cfg->password, &wifi_ap_cfg->pass_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      wifi_ap_cfg->pass_len = 0;
    }

    // Validation done later
    nvs_get_u8(nvs_handle, STR_AP_PRIMARY, &wifi_ap_cfg->primary);
    nvs_get_u8(nvs_handle, STR_AP_SECONDARY, &wifi_ap_cfg->secondary);
    nvs_get_u8(nvs_handle, STR_AP_AUTHMODE, &wifi_ap_cfg->authmode);
    nvs_get_u8(nvs_handle, STR_AP_CYPHER, &wifi_ap_cfg->cypher);
    nvs_get_u8(nvs_handle, STR_AP_HIDDEN, &wifi_ap_cfg->hidden);
    nvs_get_u8(nvs_handle, STR_AP_BANDWIDTH, &wifi_ap_cfg->bandwidth);
    nvs_get_u8(nvs_handle, STR_AP_MAX_CONN, &wifi_ap_cfg->max_connection);
    nvs_get_u8(nvs_handle, STR_AP_AUTO_OFF, &wifi_ap_cfg->auto_off);
    nvs_close(nvs_handle);
  }
  else
  {
    F_LOGE(true, true, LC_YELLOW, "Couldn't open flash for \"%s\" (func:%s, line: %d)", NVS_WIFI_AP_CFG,  __FILE__, __LINE__);
  }
}

// **************************************************************************************************
// Check for an existing WiFi configuration
// **************************************************************************************************
void get_wifi_config (void)
{
  // Check if we have saved a preferred wireless mode
  // ---------------------------------------------------
  if ( get_nvs_num(NVS_WIFI_SETTINGS, STR_WIFI_MODE, (char *)&wifi_cfg.mode, TYPE_U8) != ESP_OK )
  {
    // Default to APSTA
    wifi_cfg.mode = WIFI_MODE_APSTA;
  }

  // Check for preferred power saving
  // ---------------------------------------------------
  if ( get_nvs_num(NVS_WIFI_SETTINGS, STR_WIFI_POWERSAVE, (char *)&wifi_cfg.powersave, TYPE_U8) != ESP_OK )
  {
    wifi_cfg.powersave = WIFI_PS_MIN_MODEM;
  }

  // STA specific config
  // ---------------------------------------------------
  get_nvs_sta_cfg(&wifi_sta_cfg);

  // AP specific config
  // ---------------------------------------------------
  get_nvs_ap_cfg(&wifi_ap_cfg);
}

// **************************************************************************************************
// Start Wireless
// **************************************************************************************************
void wifi_start (void)
{
  // Set the power saving mode
  esp_wifi_set_ps((wifi_ps_type_t)wifi_cfg.powersave);

  // Log what we are doing
  F_LOGI(true, true, LC_GREEN, "WiFi mode = %s (%d)", mode2str (wifi_cfg.mode), wifi_cfg.mode);

  // Check if our mode is out-of-bounds and default to APSTA
  if ( !(wifi_cfg.mode > WIFI_MODE_NULL && wifi_cfg.mode < WIFI_MODE_MAX) )
  {
    wifi_cfg.mode = WIFI_MODE_APSTA;
  }

  // Check for valid STA configuration
  if ( BTST(wifi_cfg.mode, WIFI_MODE_STA) )
  {
    // Check if the STA SSID is set
    if ( wifi_sta_cfg.ssid_len == 0 )
    {
      F_LOGE(true, true, LC_YELLOW, "STA SSID is not set");
      wifi_cfg.mode = WIFI_MODE_AP;
    }
    // Check if the STA Password is set
    else if ( wifi_sta_cfg.pass_len < 8 )
    {
      F_LOGE(true, true, LC_YELLOW, "STA Password is too short");
      wifi_cfg.mode = WIFI_MODE_AP;
    }
    else
    {
      sta_connect(&wifi_sta_cfg);
    }
  }

  // Check if we are to run SoftAP
  if ( BTST(wifi_cfg.mode, WIFI_MODE_AP) )
  {
    start_ap();
  }
}

// **************************************************************************************************
//
// **************************************************************************************************
void init_wifi (httpd_handle_t *httpServer)
{
  F_LOGI(true, true, LC_YELLOW, "Initialising WiFi...");

  if ( wifi_event_group == NULL )
  {
    // Create our event group
    wifi_event_group = xEventGroupCreate();
  }

  if ( xApScanQueue == NULL )
  {
    xApScanQueue = xQueueCreate(3, sizeof(scan_result_t));
  }

  // Check if we had any issues
  if ( wifi_event_group == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "xEventGroupCreate() for 'wifi_event_group' failed");
  }
  else
  {
    // Save the HTTP handle we were passed
    hHttpServer = httpServer;

    // Set we have no scan in progress
    xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE);    // initialize wifi stack

    // Check for an existing configuration
    get_wifi_config();

    // initialize the tcp stack
    esp_netif_init();

    // Create event loop
    esp_event_loop_create_default();

    // Create interfaces
    netif_sta = esp_netif_create_default_wifi_sta();
    netif_ap = esp_netif_create_default_wifi_ap();

    // Set the hostname if available
    if ( hostname_len > 0 )
    {
      set_hostname(hostname);
    }

    // initialize the wifi event handlers
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_ap_handlers());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_eventHandler, NULL));

    // Initialise wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set storage and power saving
    ESP_ERROR_CHECK(esp_wifi_set_storage (WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps (WIFI_PS_NONE));

    // Start off in STA mode
    wifi_set_mode(WIFI_MODE_STA);
    ESP_ERROR_CHECK(esp_wifi_start());

    // We init SNTP here in case LWIP_DHCP_GET_NTP_SRV is set
    init_sntp();

    // Configure DHCP server
    /*
    uint32_t ip_int = StringtoIPUInt32("192.168.1.1");
    esp_netif_ip_info_t ipInfo;
    ipInfo.ip.addr = ip_int;
    ipInfo.gw.addr = ip_int;
    IP4_ADDR (&ipInfo.netmask, 255, 255, 255, 0);

    esp_netif_dhcps_stop (netif_ap);
    esp_netif_set_ip_info (netif_ap, &ipInfo);
    esp_netif_dhcps_start(netif_ap);
    */

    // Wifi initialised
    xEventGroupSetBits (wifi_event_group, WIFI_INITIALIZED);
    F_LOGI(true, true, LC_YELLOW, "WiFi initialised");

    // Start the AP or STA
    wifi_start();
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define RECEIVE_BUFFER  512
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t cgiWifiSetSta (httpd_req_t *req)
#else
esp_err_t cgiWifiSetSta (httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;
  char rcvbuf[RECEIVE_BUFFER] = {};

  if ( req->content_len > RECEIVE_BUFFER )
  {
    F_LOGE(true, true, LC_YELLOW, "Request larger than our expectations!");
  }
  else if ( httpd_req_recv (req, rcvbuf, MIN (req->content_len, RECEIVE_BUFFER)) <= 0 )
  {
    F_LOGE(true, true, LC_YELLOW, "Some kind of error happened, go figure it out");
  }
  else
  {
    char token[64] __attribute__ ((aligned(4))) = {};
    char param[128] __attribute__ ((aligned (4))) = {};
    int i, r, len;
    jsmn_parser p;
    jsmntok_t t[12];

    jsmn_init (&p);
    r = jsmn_parse(&p, rcvbuf, req->content_len, t, sizeof (t) / sizeof (t[0]));
    for ( i = 1; i < r; i++ )
    {
      snprintf(token, 63, "%.*s", t[i].end - t[i].start, rcvbuf + t[i].start);
      if ( !shrt_cmp (STR_STA_SSID, token) )
      {
        i++;
        len = t[i].end - t[i].start;
        sprintf (param, "%.*s", len, token);

        if (str_cmp (wifi_sta_cfg.ssid, param) != 0 && len < SSID_STRLEN)
        {
          if (save_nvs_str (NVS_WIFI_SETTINGS, STR_STA_SSID, param) == ESP_OK)
          {
            wifi_sta_cfg.ssid_len = len;
            memcpy (wifi_sta_cfg.ssid, param, SSID_STRLEN);
          }
        }
      }
      else if ( !shrt_cmp (STR_STA_PASSW, token) )
      {
        i++;
        len = t[i].end - t[i].start;
        sprintf (param, "%.*s", len, token);

        if (str_cmp (wifi_sta_cfg.password, param) != 0 && len < PASSW_STRLEN)
        {
          if (save_nvs_str (NVS_WIFI_SETTINGS, STR_STA_PASSW, param) == ESP_OK)
          {
            wifi_sta_cfg.pass_len = len;
            memcpy (wifi_sta_cfg.password, param, PASSW_STRLEN + 1);
          }
        }
      }
    }
  }

  httpd_resp_set_hdr (req, "Content-Type", "application/json");
  httpd_resp_send_chunk (req, JSON_SUCCESS_STR, strlen (JSON_SUCCESS_STR));
  httpd_resp_send_chunk (req, NULL, 0);

  return err;
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t cgiWifiTestSta (httpd_req_t *req)
#else
esp_err_t cgiWifiTestSta (httpd_req_t *req)
#endif
{
  char token[64]   __attribute__ ((aligned (4))) = {};
  char param[128]  __attribute__ ((aligned (4))) = {};
  char rcvbuf[RECEIVE_BUFFER] = {};
  //memset (rcvbuf, 0x0, RECEIVE_BUFFER);
  esp_err_t err = ESP_FAIL;
  wifi_sta_cfg_t sta_test = {};

  uint16_t len;
  jsmn_parser p;
  jsmntok_t t[12];

  if ( req->content_len > RECEIVE_BUFFER )
  {
    F_LOGE (true, true, LC_YELLOW, "Request larger than our expectations!");
  }
  else if ( httpd_req_recv (req, rcvbuf, MIN (req->content_len, RECEIVE_BUFFER)) <= 0 )
  {
    F_LOGE (true, true, LC_YELLOW, "Some kind of error happened, go figure it out");
  }
  else
  {
    jsmn_init (&p);
    int r = jsmn_parse (&p, rcvbuf, req->content_len, t, sizeof (t) / sizeof (t[0]));
    if ( r < 0 )
    {
      F_LOGE (true, true, LC_RED, "jsmn_parse error (%d), data: %.*s", r, req->content_len, rcvbuf);
    }
    else
    {
      uint16_t i = 1;
      for ( i = 1; i < r; i++ )
      {
        snprintf (token, 63, "%.*s", t[i].end - t[i].start, rcvbuf + t[i].start);
        if ( !shrt_cmp (STR_STA_SSID, token) )
        {
          i++;
          if ( t[i].end - t[i].start <= SSID_STRLEN )
          {
            sta_test.ssid_len = t[i].end - t[i].start;
            sprintf (sta_test.ssid, "%.*s", sta_test.ssid_len, rcvbuf + t[i].start);
          }
        }
        else if ( !shrt_cmp (STR_STA_BSSID, token) )
        {
          i++;
          if ( t[i].end - t[i].start == BSSID_STRLEN )
          {
            sprintf (param, "%.*s", t[i].end - t[i].start, rcvbuf + t[i].start);
            sta_test.bssid_set = str2mac (param, sta_test.bssid);
          }
        }
          else if ( !shrt_cmp (STR_STA_PASSW, token) )
        {
          i++;
          if ( t[i].end - t[i].start <= PASSW_STRLEN )
          {
            sta_test.pass_len = t[i].end - t[i].start;
            sprintf (sta_test.password, "%.*s", sta_test.pass_len, rcvbuf + t[i].start);
          }
        }
      }
    }
  }

  F_LOGI (true, true, LC_MAGENTA, "S: %s (%d chars), B: %s (set: %d), P: %s (%d chars)", sta_test.ssid, sta_test.ssid_len, mac2str (sta_test.bssid), sta_test.bssid_set, sta_test.password, sta_test.pass_len);

  /* Make sure we are disconnected before testing */
  err = esp_wifi_disconnect ();
  if ( err == 0 )
  {
    err = ESP_FAIL;
    xEventGroupSetBits (wifi_event_group, WIFI_TEST_STA_CFG);
    if ( sta_connect (&sta_test) )
    {
      // Wait for connection or other
      int t = 5;
      while ( t-- > 0 )
      {
        EventBits_t uxBits = xEventGroupWaitBits (wifi_event_group, WIFI_STA_CONNECTED, false, false, 10000);
        F_LOGI (true, true, LC_YELLOW, "tick");
        if ( BTST (uxBits, WIFI_STA_CONNECTED) )
        {
          F_LOGI (true, true, LC_MAGENTA, "connected");
          err = ESP_OK;
          break;
        }
      }
    }
    xEventGroupClearBits (wifi_event_group, WIFI_TEST_STA_CFG);
  }

  F_LOGW (true, true, LC_MAGENTA, "err = %d", err);

  httpd_resp_set_hdr (req, "Content-Type", "application/json");
  httpd_resp_send_chunk (req, JSON_SUCCESS_STR, strlen (JSON_SUCCESS_STR));
  httpd_resp_send_chunk (req, NULL, 0);

  return err;
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t cgiWifiSetAp (httpd_req_t *req)
#else
esp_err_t cgiWifiSetAp (httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;
  char rcvbuf[RECEIVE_BUFFER] = {};

  if ( req->content_len > RECEIVE_BUFFER )
  {
    F_LOGE(true, true, LC_YELLOW, "Request larger than our expectations!");
  }
  else if ( httpd_req_recv (req, rcvbuf, MIN (req->content_len, RECEIVE_BUFFER)) <= 0 )
  {
    F_LOGE(true, true, LC_YELLOW, "Some kind of error happened, go figure it out");
  }
  else
  {
    struct yuarel_param params[MAX_URI_PARTS];
    char *ssid;
    char *pass;
    int pc = 0;

    if ( (pc = yuarel_parse_query (rcvbuf, '&', params, MAX_URI_PARTS)) )
    {
      while ( pc-- > 0 )
      {
        if ( !str_cmp (STR_STA_SSID, params[pc].key) )
        {
          ssid = params[pc].val;
        }
        else if ( !str_cmp (STR_STA_PASSW, params[pc].key) )
        {
          pass = params[pc].val;
        }
      }
    }

    if ( ssid )
    {
    }
  }

  httpd_resp_set_hdr (req, "Content-Type", "application/json");
  httpd_resp_send_chunk (req, JSON_SUCCESS_STR, strlen (JSON_SUCCESS_STR));
  httpd_resp_send_chunk (req, NULL, 0);

  return err;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t  cgiWifiSetMode (httpd_req_t *req)
#else
esp_err_t  cgiWifiSetMode (httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;

  //printf("req->uri: %s (%d)\n", req->uri, req->content_len);

  httpd_resp_set_hdr (req, "Content-Type", "application/json");
  httpd_resp_send_chunk (req, JSON_SUCCESS_STR, strlen (JSON_SUCCESS_STR));
  httpd_resp_send_chunk (req, NULL, 0);

  return err;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t tplWifi (struct async_resp_arg *resp_arg, char *token)
#else
esp_err_t tplWifi (httpd_req_t *req, char *token, void **arg)
#endif
{
  esp_err_t err = ESP_FAIL;


  return err;
}

// --------------------------------------------------------------------------
//
// **************************************************************************************************
int networkIsConnected()
{
  return xEventGroupGetBits (wifi_event_group) & WIFI_STA_CONNECTED;
}

int waitConnectedBit()
{
  return xEventGroupWaitBits (wifi_event_group, WIFI_STA_CONNECTED, false, true, portMAX_DELAY);
}

// ===============================================================================================
// from Kolban's Book
// You will also need to call esp_wifi_set_config() to specify the configuration parameters.
// If we are being an access point, we will not actually start listening for connections
// until after esp_wifi_start().
// if we are being a station, we will not connect to an access point until after a call
// to esp_wifi_start() and then esp_wifi_connect().
// ===============================================================================================
//             mode
// curr_mode   none   STA    AP    APSTA
// none        --     c      ?     c
// STA         d      --     d     --
// AP          ?      c      --    c
// APSTA       d      ---    d     --
// ===============================================================================================
esp_err_t wifi_set_mode(wifi_mode_t mSet)
{
  esp_err_t err = ESP_OK;
  wifi_mode_t  cMode;

  F_LOGV(true, true, LC_GREY, "wifi_set_mode(%s)", mode2str(mSet));

  err = esp_wifi_set_mode(mSet);

  esp_wifi_get_mode(&cMode);
  F_LOGV(true, true, LC_GREY, "actual mode: %s", mode2str(cMode));

  return err;
}

// **************************************************************************************************
//
// **************************************************************************************************
int wifi_get_IpStr (char *buf, int buflen)
{
  return snprintf (buf, buflen, ipAddr);
}

esp_err_t wifi_get_hostname (const char **hostName)
{
  esp_err_t err = ESP_OK;

  if ( wifi_cfg.mode & WIFI_MODE_AP )
  {
    err = esp_netif_get_hostname (netif_ap, hostName);
  }
  else
  {
    err = esp_netif_get_hostname (netif_sta, hostName);
  }

  return err;
}
// **************************************************************************************************
//
// **************************************************************************************************
esp_err_t wifi_set_channel (uint8_t primary, wifi_second_chan_t second)
{
  return esp_wifi_set_channel (primary, second);
}

// *************************************************************************************
// Configuration control functions
// *************************************************************************************

// --------------------------------------------------------------------------
// WiFi Config
// --------------------------------------------------------------------------
int _set_ap_setting (char *buf, int bufsize, char *param, char *value, int setting)
{
  tNumber saveType = TYPE_NONE;

  //F_LOGI(true, true, LC_GREY, "param: %s, value: %s", param, value);

  //uint8_t os = 0;
  uint8_t ns = atoi(value);

  switch ( (ap_type_t)setting )
  {
    case AP_IP_ADDR:
      break;
    case AP_SSID:
      if ( strcmp(value, wifi_ap_cfg.ssid) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(wifi_ap_cfg.ssid, value, SSID_STRLEN);
      }
      break;
    case AP_PASSWORD:
      if ( strcmp(value, wifi_ap_cfg.password) != 0 )
      {
        saveType = TYPE_STR;
        memcpy(wifi_ap_cfg.password, value, PASSW_STRLEN);
      }
      break;
    case AP_PRIMARY:
      if ( (ns > 0 && ns <= 12) && wifi_ap_cfg.primary != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.primary = ns;
      }
      break;
      break;
    case AP_SECONDARY:
      if ( (ns > 0 && ns <= 12) && wifi_ap_cfg.secondary != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.secondary = ns;
      }
      break;
      break;
    case AP_AUTHMODE:
      if ( ns < WIFI_AUTH_MAX && wifi_ap_cfg.authmode != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.authmode = ns;
      }
      break;
    case AP_CYPHER:
      if ( (ns > 0 && ns <= WIFI_CIPHER_TYPE_AES_GMAC256) && wifi_ap_cfg.cypher != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.cypher = ns;
      }
      break;
    case AP_HIDDEN:
      if ( ns <= 1 && wifi_ap_cfg.hidden != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.hidden = ns;
      }
      break;
    case AP_BANDWIDTH:
      if ( (ns >= WIFI_BW_HT20 && ns <= WIFI_BW_HT40) && wifi_ap_cfg.authmode != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.bandwidth = ns;
      }
      break;
    case AP_AUTO_OFF:
      if ( (ns <= 1) && wifi_ap_cfg.auto_off != ns )
      {
        saveType = TYPE_U8;
        wifi_ap_cfg.auto_off = ns;
      }
      break;
  }
  esp_err_t err = ESP_FAIL;
  if ( saveType == TYPE_STR )
  {
    err = save_nvs_str(NVS_WIFI_AP_CFG, param, value);
  }
  else if ( saveType != TYPE_NONE )
  {
    err = save_nvs_num(NVS_WIFI_AP_CFG, param, value, saveType);
  }

  return(snprintf(buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}
int _get_ap_setting (char *buf, int bufsize, int setting)
{
  uint16_t len = 0;
  switch ( (ap_type_t)setting )
  {
    case AP_IP_ADDR:
      break;
    case AP_SSID:
      len = snprintf (buf, bufsize, "%s", wifi_ap_cfg.ssid);
      break;
    case AP_PASSWORD:
      len = snprintf (buf, bufsize, "%s", wifi_ap_cfg.password);
      break;
    case AP_PRIMARY:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.primary);
      break;
    case AP_SECONDARY:
      break;
    case AP_AUTHMODE:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.authmode);
      break;
    case AP_CYPHER:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.cypher);
      break;
    case AP_HIDDEN:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.hidden);
      break;
    case AP_BANDWIDTH:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.bandwidth);
      break;
    case AP_AUTO_OFF:
      len = snprintf (buf, bufsize, "%d", wifi_ap_cfg.auto_off);
      break;
  }
  return len;
}
// ***********************************************
int _set_sta_setting (char *buf, int bufsize, char *param, char *value, int setting)
{
  bool saveParam = false;

  F_LOGI(true, true, LC_GREY, "param: %s, value: %s", param, value);

  switch ( (sta_type_t)setting )
  {
    case STA_SSID:
      if ( strcmp (value, wifi_sta_cfg.ssid) != 0 )
      {
        saveParam = true;
        memcpy (wifi_sta_cfg.ssid, value, SSID_STRLEN);
      }
      break;
    case STA_USERNAME:
      break;
    case STA_PASSWORD:
      if ( strcmp (value, wifi_sta_cfg.password) != 0 )
      {
        saveParam = true;
        memcpy (wifi_sta_cfg.password, value, PASSW_STRLEN);
      }
      break;
    case STA_IP_ADDR:
      break;
    case STA_NTP_ADDR:
      break;
  }

  esp_err_t err = ESP_FAIL;
  if ( saveParam )
  {
    err = save_nvs_str (NVS_WIFI_STA_CFG, param, value);
  }

  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}
int _get_sta_setting (char *buf, int bufsize, int setting)
{
  uint16_t len = 0;
  switch ( (sta_type_t)setting )
  {
    case STA_SSID:
      len = snprintf (buf, bufsize, "%s", wifi_sta_cfg.ssid);
      break;
    case STA_USERNAME:
      break;
    case STA_PASSWORD:
      //len = snprintf (buf, bufsize, "%s", wifi_sta_cfg.password);
      len = snprintf (buf, bufsize, "%s", "********");
      break;
    case STA_IP_ADDR:
      break;
    case STA_NTP_ADDR:
      break;
  }
  return len;
}
// ***********************************************
int _set_wifi_setting (char *buf, int bufsize, char *param, char *value, int setting)
{
  bool saveParam = false;
  uint8_t ns = atoi (value);
  switch ( (wifi_type_t)setting )
  {
    case WIFI_MODE:
      if ( ns < WIFI_MODE_MAX && wifi_cfg.mode != ns )
      {
        saveParam = true;
        wifi_cfg.mode = (wifi_mode_t)ns;
      }
      break;
    case WIFI_POWERSAVE:
      if ( ns <= WIFI_PS_MAX_MODEM && wifi_cfg.powersave != ns )
      {
        saveParam = true;
        wifi_cfg.powersave = (wifi_ps_type_t)ns;
      }
      break;
  }
  esp_err_t err = ESP_FAIL;
  if ( saveParam )
  {
    err = save_nvs_num (NVS_WIFI_SETTINGS, param, value, TYPE_U8);
  }
  return(snprintf (buf, bufsize, JSON_RESPONSE_AS_INTVAL, param, err));
}
int _get_wifi_setting (char *buf, int bufsize, int setting)
{
  uint16_t len = 0;
  switch ( (wifi_type_t)setting )
  {
    case WIFI_MODE:
      len = snprintf (buf, bufsize, "%d", wifi_cfg.mode);
      break;
    case WIFI_POWERSAVE:
      len = snprintf (buf, bufsize, "%d", wifi_cfg.powersave);
      break;
  }
  return len;
}

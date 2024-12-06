
#ifndef __APP_WIFI_H__
#define __APP_WIFI_H__

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include "app_httpd.h"

#define NUMARRAY_SIZE           16
#define SSID_STRLEN             32
#define BSSID_STRLEN            17
#define PASSW_STRLEN            64
#define UNAME_STRLEN            64
#define STR_STA_SSID            "sta_ssid"
#define STR_STA_BSSID           "sta_bssid"
#define STR_STA_UNAME           "sta_username"
#define STR_STA_PASSW           "sta_password"
#define STR_STA_IP_ADDR         "sta_ipaddr"
#define STR_STA_NTP_ADDR        "sta_ntp_addr"

#define STR_AP_SSID             "ap_ssid"
#define STR_AP_PASSW            "ap_password"
#define STR_AP_PRIMARY          "ap_primary"
#define STR_AP_SECONDARY        "ap_secondary"
#define STR_AP_AUTHMODE         "ap_authmode"
#define STR_AP_CYPHER           "ap_cypher"
#define STR_AP_HIDDEN           "ap_hidden"
#define STR_AP_PROTOCOL         "ap_protocol"
#define STR_AP_BANDWIDTH        "ap_bandwidth"
#define STR_AP_MAX_CONN         "ap_connections"
#define STR_AP_IP_ADDR          "ap_ipaddr"
#define STR_AP_AUTO_OFF         "ap_autooff"

#define STR_WIFI_MODE           "wifi_mode"
#define STR_WIFI_POWERSAVE      "wifi_powersave"

typedef enum
{
  STA_SSID,
  STA_USERNAME,
  STA_PASSWORD,
  STA_IP_ADDR,
  STA_NTP_ADDR
} sta_type_t;

typedef enum
{
  AP_IP_ADDR,
  AP_SSID,
  AP_PASSWORD,
  AP_PRIMARY,
  AP_SECONDARY,
  AP_AUTHMODE,
  AP_CYPHER,
  AP_HIDDEN,
  AP_BANDWIDTH,
  AP_AUTO_OFF
} ap_type_t;

typedef enum
{
  WIFI_MODE,
  WIFI_POWERSAVE
} wifi_type_t;

typedef struct
{
  char        ssid[SSID_STRLEN];
  char        bssid[BSSID_STRLEN];
  char        username[UNAME_STRLEN];
  char        password[PASSW_STRLEN];
} sta_auth_t;

typedef struct
{
  char         ssid[SSID_STRLEN] __attribute__ ((aligned (4)));   // Station SSID
  char         bssid[BSSID_STRLEN] __attribute__ ((aligned (4))); // Station BSSID
  char         username[UNAME_STRLEN];                            // Station username
  char         password[PASSW_STRLEN];                            // Station pwassword
  char         ip_addr[64];                                       // Station IP address
  size_t       ssid_len;                                          // Store len
  size_t       uname_len;                                         // Store len
  size_t       pass_len;                                          // Store len
} wifi_sta_cfg_t;

typedef struct
{
  char         ssid[SSID_STRLEN] __attribute__ ((aligned(4)));    // AP SSID
  char         password[PASSW_STRLEN];                            // AP Password
  size_t       ssid_len;                                          // Store len
  size_t       pass_len;                                          // Store len
  //uint8_t      enabled;                                           // Enabled by default?
  uint8_t      primary;                                           // AP Primary Channel
  uint8_t      secondary;                                         // AP's Second Channel
  uint8_t      authmode;                                          // AP Authmode
  uint8_t      cypher;                                            // AP Cyphe
  uint8_t      hidden;                                            // Hidden AP?
  uint8_t      protocol;                                          // AP Protocol
  uint8_t      bandwidth;                                         // AP Bandwidth (HT20 / HT40)
  uint8_t      max_connection;                                    // Maximum number of clients
  uint8_t      auto_off;                                          // Auto stop AP when STA connects
} wifi_ap_cfg_t;

typedef struct
{
  wifi_mode_t  mode __attribute__ ((packed));                     // 0xA1 - WIFI_MODE (small as possible)
  uint8_t      powersave;                                         // 0xA2 - WIFI_POWERSAVE
} wifi_cfg_t;


// --------------------------------------------------------------------------
// Scan result
// --------------------------------------------------------------------------
typedef struct 
{
  uint16_t          apCount;
  wifi_ap_record_t  *apList; //apList[CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES];
} scan_result_t;

//extern ScanResultData cgiWifiAps;

#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t   tplWifi(struct async_resp_arg *resp_arg, char *token);
#else
esp_err_t   tplWifi(httpd_req_t *req, char *token, void **arg);
#endif

extern size_t  hostname_len;
extern char    hostname[64];

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
int         networkIsConnected();
int         waitConnectedBit();

const char *auth2str(int auth);
const char *mode2str(int mode);
const char *SecondChanStr(wifi_second_chan_t second);

bool        wifi_connect(void);
void        wifi_disconnect(void);
void        wifi_startScan (void);
esp_err_t   wifi_getApScanResult (scan_result_t *cgiWifiAps);

#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t   tplWifi(struct async_resp_arg *resp_arg, char *token);
#else
esp_err_t   tplWifi(httpd_req_t *req, char *token, void **arg);
#endif
esp_err_t   cgiWifiSetSta(httpd_req_t *req);
esp_err_t   cgiWifiTestSta (httpd_req_t* req);
esp_err_t   cgiWifiSetAp(httpd_req_t *req);
esp_err_t   cgiWifiSetMode(httpd_req_t *req);

void        init_wifi(httpd_handle_t *httpServer);

esp_err_t   wifi_set_mode(wifi_mode_t mode);
int         wifi_get_IpStr(char *buf, int buflen);
esp_err_t   wifi_get_hostname(const char **hostName);
esp_err_t   wifi_set_Channel(uint8_t primary, wifi_second_chan_t second);

// ***********************************************
int        _set_ap_setting (char *buf, int bufsize, char *param, char *value, int zone);
int        _get_ap_setting (char *buf, int bufsize, int unused);
// ***********************************************
int        _set_sta_setting (char *buf, int bufsize, char *param, char *value, int zone);
int        _get_sta_setting (char *buf, int bufsize, int unused);
// ***********************************************
int        _test_sta_setting (char *buf, int bufsize, char *param, char *value, int setting);
// ***********************************************
int        _set_wifi_setting (char *buf, int bufsize, char *param, char *value, int zone);
int        _get_wifi_setting (char *buf, int bufsize, int unused);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif // __APP_WIFI_H__

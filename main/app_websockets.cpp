

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_http_server.h>
#include <espfs_webpages.h>

#include "app_main.h"
#include "app_wifi.h"
#include "app_utils.h"
#include "app_websockets.h"
#include "cgi_lightcontrol.h"
#include "app_lightcontrol.h"
#include "device_control.h"
#include "app_yuarel.h"
#include "app_httpd.h"
#include "app_mqtt.h"

// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------
#define MAX_CLIENTS     12

// --------------------------------------------------------------------------
// Globals
// --------------------------------------------------------------------------
wss_keep_alive_storage_t *ws_client_control = NULL;

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_async_frame (async_resp_arg *resp_arg, uint8_t *msg, uint16_t msglen)
#else
static void send_async_frame (async_resp_arg *resp_arg, uint8_t *msg, uint16_t msglen)
#endif
{
  httpd_ws_frame_t ws_pkt = {};
  ws_pkt.payload = msg;
  ws_pkt.len     = msglen;
  ws_pkt.type    = HTTPD_WS_TYPE_TEXT;

  httpd_ws_send_frame_async (resp_arg->hd, resp_arg->fd, &ws_pkt);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define BUF_SIZE  2048
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static int prepareSystemStatusMsg (char **buf)
#else
static int prepareSystemStatusMsg (char **buf)
#endif
{
  char jsonBuffer[BUF_SIZE+1];
  uint16_t bufptr = 0;

  // create tm struct
  time_t now;
  time (&now);

  uint64_t uptime = esp_timer_get_time () / 1000000;
  uint16_t dys    = uptime / 86400;
  uint16_t hrs    = (uptime % 86400) / 3600;
  uint16_t mns    = (uptime % 3600) / 60;
  uint16_t scs    = (uptime % 3600) % 60;

  int sec_light = lightStatus (SECURITY_LIGHT);
  int heap = 0;                 //(int) system_get_free_heap_size();

  /*
   * Generate response in JSON format-
   */
  bufptr += snprintf (jsonBuffer, BUF_SIZE, "{\"dt\":\"%.19s\",", ctime (&now));

  // ------------------------------------------------
  // Terse names to reduce bytes over the air
  // ------------------------------------------------
  bufptr += snprintf (&jsonBuffer[bufptr], BUF_SIZE - bufptr,
                    " \"ut\": \"%03d:%02d:%02d:%02d\","   // uptime
                    " \"me\": %d,"                        // memory
                    " \"cp\": %d,"                        // current_pattern
                    " \"sl\": %d,"                        // sec_light
                    " \"ll\": \"%s\","                    // schedule
                    " \"pa\": %d,"                        // paused ( 0 = no, 1 = true)
                    " \"di\": %d,"                        // dim
                    " \"z0\": %d,"                        // zone0
                    " \"z1\": %d,"                        // zone1
                    " \"z2\": %d,"                        // zone2
                    " \"z3\": %d,"                        // zone3
                    " \"hp\": %d}",                       // heap
                    dys, hrs, mns, scs, xPortGetFreeHeapSize (), control_vars.cur_pattern, sec_light, int2sched ((schedule)control_vars.schedule),
                    BTST(control_vars.bitflags, DISP_BF_PAUSED), control_vars.dim, zoneStatus (0), zoneStatus (1), zoneStatus (2), zoneStatus (3), heap);

  F_LOGV(true, true, LC_BRIGHT_GREEN, "%s", jsonBuffer);

  *(buf) = strndup(jsonBuffer, bufptr);
  return bufptr;
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_schedule (async_resp_arg *resp_arg)
#else
static void send_schedule (async_resp_arg *resp_arg)
#endif
{
  char  *buf = NULL;
  int buflen = 0;
  send_async_frame (resp_arg, (uint8_t *)buf, buflen);

  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "d) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_status_update (async_resp_arg *resp_arg)
#else
static void send_status_update (async_resp_arg *resp_arg)
#endif
{
  char  *buf = NULL;
  int buflen = prepareSystemStatusMsg(&buf);
  send_async_frame (resp_arg, (uint8_t *)buf, buflen);
  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "e) Releasing allocation 0x%08X", (unsigned int)buf);
  free (buf);

  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "f) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_tasks_update (async_resp_arg *resp_arg)
#else
static void send_tasks_update (async_resp_arg *resp_arg)
#endif
{
  char  *buf = NULL;
  int buflen = getSystemTasksJsonString(&buf);
  send_async_frame (resp_arg, (uint8_t *)buf, buflen);

  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "g) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_log_messages(async_resp_arg *resp_arg)
#else
static void send_log_messages(async_resp_arg *resp_arg)
#endif
{
  char  *buf = NULL;
  int buflen = get_log_messages(&buf, (uint32_t *)resp_arg->voidPtr);

  if ( buflen > 0 )
  {
    send_async_frame (resp_arg, (uint8_t *)buf, buflen);

    //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "h) Releasing allocation 0x%08X", (unsigned int)buf);
    vPortFree (buf);
    buf = NULL;
  }
  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "i) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_eng_data (async_resp_arg *resp_arg)
#else
static void send_eng_data (async_resp_arg *resp_arg)
#endif
{
  char  *buf = NULL;
  int buflen = getEngDataJsonString (&buf);
  send_async_frame (resp_arg, (uint8_t *)buf, buflen);

  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "j) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
IRAM_ATTR static uint64_t _tick_get_ms (void)
{
  return esp_timer_get_time () / 1000;
}

// --------------------------------------------------------------------------
// Goes over active clients to find out how long we could sleep before checking who's alive
// --------------------------------------------------------------------------
static uint64_t get_max_delay (wss_keep_alive_t h)
{
  int64_t check_after_ms = WS_KA_DECEASED; // max delay, no need to check anyone

  for ( int i = 0; i < h->max_clients; ++i )
  {
    if ( h->clients[i].type == CLIENT_ACTIVE )
    {
      uint64_t check_this_client_at = h->clients[i].last_seen + h->keep_alive_period_ms;
      if ( check_this_client_at < check_after_ms + _tick_get_ms () )
      {
        check_after_ms = check_this_client_at - _tick_get_ms ();
        if ( check_after_ms < 0 )
        {
          check_after_ms = WS_KA_PERIOD;
        }
      }
    }
  }

  return check_after_ms;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static bool add_new_client (wss_keep_alive_t h, int sockfd)
#else
static bool add_new_client (wss_keep_alive_t h, int sockfd)
#endif
{
  bool success = false;

  for ( int i = 0; i < h->max_clients && !success; ++i )
  {
    if ( h->clients[i].type == NO_CLIENT )
    {
      h->clients[i].type      = CLIENT_ACTIVE;
      h->clients[i].ws_info   = WS_NONE;
      h->clients[i].fd        = sockfd;
      h->clients[i].user_int  = 0;
      h->clients[i].last_seen = _tick_get_ms ();
      success = true;
    }
  }

  return success;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static bool update_client (wss_keep_alive_t h, int sockfd, uint64_t timestamp)
#else
static bool update_client (wss_keep_alive_t h, int sockfd, uint64_t timestamp)
#endif
{
  bool success = false;

  for ( int i = 0; i < h->max_clients && !success; ++i )
  {
    if ( h->clients[i].type == CLIENT_ACTIVE && h->clients[i].fd == sockfd )
    {
      h->clients[i].last_seen = timestamp;
      success = true;
    }
  }

  return success;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static bool remove_client (wss_keep_alive_t h, int sockfd)
#else
static bool remove_client (wss_keep_alive_t h, int sockfd)
#endif
{
  bool success = false;

  for ( int i = 0; i < h->max_clients && !success; ++i )
  {
    if ( h->clients[i].type == CLIENT_ACTIVE && h->clients[i].fd == sockfd )
    {
      h->clients[i].type = NO_CLIENT;
      h->clients[i].fd   = -1;
      success = true;
    }
  }

  return success;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void send_ping (void *arg)
#else
static void send_ping (void *arg)
#endif
{
  struct async_resp_arg *resp_arg = (async_resp_arg *)arg;

  httpd_ws_frame_t ws_pkt = {};
  ws_pkt.payload = NULL;
  ws_pkt.len     = 0;
  ws_pkt.type    = HTTPD_WS_TYPE_PING;

  httpd_ws_send_frame_async (resp_arg->hd, resp_arg->fd, &ws_pkt);

  free (resp_arg);
}

#define JSON_SCAN_STR "{\"ssid\": \"%s\",\"bssid\": \"%02X:%02X:%02X:%02X:%02X:%02X\",\"chan\": %d,\"2chan\": %d,\"rssi\": %d,\"enc\": \"%s\"}"
#define SIZE_JSONBUF   4096
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR char *ws_scan_results (uint16_t *strLen)
#else
char *ws_scan_results (uint16_t *strLen)
#endif
{
  scan_result_t cgiWifiAps = {0, 0};
  char *jsonStr = NULL;
  *strLen = 0;

  wifi_ap_record_t apinfo;
  esp_wifi_sta_get_ap_info(&apinfo);

  wifi_getApScanResult(&cgiWifiAps);

  // Have we got any AP's from the last scan?
  if ( cgiWifiAps.apCount > 0 )
  {
    // Allocate some ram for the JSON response
    jsonStr = (char *)pvPortMalloc(SIZE_JSONBUF+1);
    memset (jsonStr, 0x0, (SIZE_JSONBUF+1));

    // Start our JSON response
    *strLen = snprintf (jsonStr, SIZE_JSONBUF - *strLen, "{\"bssid\": \"%02X:%02X:%02X:%02X:%02X:%02X\",\"ssid\":\"%s\",\"APs\": [",
                    apinfo.bssid[0], apinfo.bssid[1], apinfo.bssid[2], apinfo.bssid[3], apinfo.bssid[4], apinfo.bssid[5], apinfo.ssid);

    // print the list
    for ( int i = 0; i < cgiWifiAps.apCount; i++ )
    {
      // Comma seperate JSON objects
      if ( (jsonStr[(*strLen - 1)]) == '}' )
      {
        *strLen += snprintf (&jsonStr[*strLen], SIZE_JSONBUF - *strLen, ", ");
      }
      *strLen += snprintf (&jsonStr[*strLen], SIZE_JSONBUF - *strLen, JSON_SCAN_STR,
        cgiWifiAps.apList[i].ssid,
        cgiWifiAps.apList[i].bssid[0], cgiWifiAps.apList[i].bssid[1], cgiWifiAps.apList[i].bssid[2], cgiWifiAps.apList[i].bssid[3], cgiWifiAps.apList[i].bssid[4], cgiWifiAps.apList[i].bssid[5],
        cgiWifiAps.apList[i].primary, cgiWifiAps.apList[i].second,
        cgiWifiAps.apList[i].rssi, auth2str (cgiWifiAps.apList[i].authmode));
    }
    *strLen += snprintf (&jsonStr[*strLen], SIZE_JSONBUF - *strLen, "]}");
  }

  // Release memory
  if ( cgiWifiAps.apList != NULL )
  {
    //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "k) Releasing allocation 0x%08X", (unsigned int)cgiWifiAps.apList);
    vPortFree (cgiWifiAps.apList);
    cgiWifiAps.apList = NULL;
  }

  return jsonStr;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void wss_keep_alive_stop (wss_keep_alive_t h)
#else
void wss_keep_alive_stop (wss_keep_alive_t h)
#endif
{
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  client_fd_action_t stop = {.type = STOP_TASK};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour
  xQueueSendToBack (h->q, &stop, 0);
  // internal structs will be de-allocated in the task
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t wss_keep_alive_add_client (wss_keep_alive_t h, int fd)
#else
esp_err_t wss_keep_alive_add_client (wss_keep_alive_t h, int fd)
#endif
{
  esp_err_t err = ESP_FAIL;
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  client_fd_action_t client_fd_action = {
    .type    = CLIENT_FD_ADD,
    .fd      = fd
  };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

  if ( xQueueSendToBack (h->q, &client_fd_action, 0) == pdTRUE )
  {
    err = ESP_OK;
  }

  F_LOGV(true, true, LC_BRIGHT_GREEN, "err = %s", (err == ESP_OK)?"False":"True");

  return err;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t wss_keep_alive_remove_client (wss_keep_alive_t h, int fd)
#else
esp_err_t wss_keep_alive_remove_client (wss_keep_alive_t h, int fd)
#endif
{
  esp_err_t err = ESP_FAIL;
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  client_fd_action_t client_fd_action = {
    .type = CLIENT_FD_REMOVE,
    .fd   = fd
  };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

  if ( xQueueSendToBack(h->q, &client_fd_action, 0) == pdTRUE )
  {
    err = ESP_OK;
  }

  F_LOGV(true, true, LC_BRIGHT_GREEN, "err = %s", (err == ESP_OK)?"False":"True");

  return err;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t wss_keep_alive_client_is_active (wss_keep_alive_t h, int fd)
#else
esp_err_t wss_keep_alive_client_is_active (wss_keep_alive_t h, int fd)
#endif
{
  esp_err_t err = ESP_FAIL;
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  client_fd_action_t client_fd_action = {
    .type      = CLIENT_UPDATE,
    .fd        = fd,
    .last_seen = _tick_get_ms()
  };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

  if ( xQueueSendToBack(h->q, &client_fd_action, 0) == pdTRUE )
  {
    err = ESP_OK;
  }

  return err;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR bool wss_keep_alive_client_set_data (wss_keep_alive_t h, int fd, uint32_t value)
#else
bool wss_keep_alive_client_set_data (wss_keep_alive_t h, int fd, uint32_t value)
#endif
{
  bool success = false;

  F_LOGV(true, true, LC_BRIGHT_GREEN, "h = %p", h);

  for ( int i = 0; i < h->max_clients && !success; ++i )
  {
    if ( h->clients[i].type == CLIENT_ACTIVE && h->clients[i].fd == fd )
    {
      h->clients[i].ws_info  = (ws_info_type_t)value;
      h->clients[i].user_int = 0;
      success = true;
    }
  }

  return success;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void keep_alive_task(void *arg)
#else
static void keep_alive_task(void *arg)
#endif
{
  wss_keep_alive_storage_t *keep_alive_storage = (wss_keep_alive_storage_t *)arg;
  client_fd_action_t client_action;
  bool run_task = true;

  while ( run_task )
  {
    if ( xQueueReceive(keep_alive_storage->q, (void *)&client_action, get_max_delay(keep_alive_storage) / portTICK_PERIOD_MS) == pdTRUE )
    {
      switch ( client_action.type )
      {
        case CLIENT_FD_ADD:
          if ( !add_new_client(keep_alive_storage, client_action.fd) )
          {
            F_LOGE(true, true, LC_YELLOW, "Cannot add new client");
          }
          break;
        case CLIENT_FD_REMOVE:
          if ( !remove_client(keep_alive_storage, client_action.fd) )
          {
            F_LOGE(true, true, LC_YELLOW, "Cannot remove client fd:%d", client_action.fd);
          }
          break;
        case CLIENT_UPDATE:
          if ( !update_client(keep_alive_storage, client_action.fd, client_action.last_seen) )
          {
            F_LOGE(true, true, LC_YELLOW, "Cannot find client fd:%d", client_action.fd);
          }
          break;
        case STOP_TASK:
          run_task = false;
          break;
        default:
          F_LOGE(true, true, LC_YELLOW, "Unexpected client action");
          break;
      }
    }
    else
    {
      // timeout: check if PING message needed
      for ( int i = 0; i < keep_alive_storage->max_clients; ++i )
      {
        if ( keep_alive_storage->clients[i].type == CLIENT_ACTIVE )
        {
          if ( keep_alive_storage->clients[i].last_seen + keep_alive_storage->keep_alive_period_ms <= _tick_get_ms () )
          {
            if ( keep_alive_storage->clients[i].last_seen + keep_alive_storage->not_alive_after_ms <= _tick_get_ms () )
            {
              keep_alive_storage->client_not_alive_cb (keep_alive_storage, keep_alive_storage->clients[i].fd);
            }
            else
            {
              keep_alive_storage->check_client_alive_cb (keep_alive_storage, keep_alive_storage->clients[i].fd);
            }
          }
        }
      }
    }
  }
  vQueueDelete(keep_alive_storage->q);
  free(keep_alive_storage);

  vTaskDelete(NULL);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
wss_keep_alive_t wss_keep_alive_start(wss_keep_alive_config_t *config)
{
  size_t queue_size       = config->max_clients / 2;
  size_t client_list_size = config->max_clients + queue_size;
  ws_client_control       = (wss_keep_alive_storage_t *)pvPortMalloc(sizeof (wss_keep_alive_storage_t) + client_list_size * sizeof (client_fd_action_t));

  if ( ws_client_control != NULL )
  {
    ws_client_control->check_client_alive_cb = config->check_client_alive_cb;
    ws_client_control->client_not_alive_cb   = config->client_not_alive_cb;
    ws_client_control->max_clients           = config->max_clients;
    ws_client_control->not_alive_after_ms    = config->not_alive_after_ms;
    ws_client_control->keep_alive_period_ms  = config->keep_alive_period_ms;
    ws_client_control->user_ctx              = config->user_ctx;
    ws_client_control->q                     = xQueueCreate(queue_size, sizeof(client_fd_action_t));
#if CONFIG_APP_ALL_CORES
    xTaskCreate (keep_alive_task, "keep_alive_task", config->task_stack_size, ws_client_control, config->task_prio, NULL);
#else
    xTaskCreatePinnedToCore (keep_alive_task, "keep_alive_task", config->task_stack_size, ws_client_control, config->task_prio, NULL, TASKS_CORE);
#endif

#if CONFIG_APP_ALL_CORES
    xTaskCreate (wss_server_send_messages, TASK_NAME_WSS, STACKSIZE_WSS, ws_client_control, TASK_PRIORITY_WSS, NULL);
#else
    xTaskCreatePinnedToCore (wss_server_send_messages, TASK_NAME_WSS, STACKSIZE_WSS, ws_client_control, TASK_PRIORITY_WSS, NULL, TASKS_CORE);
#endif
  }

  return ws_client_control;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR bool client_not_alive_cb (wss_keep_alive_t h, int fd)
#else
bool client_not_alive_cb (wss_keep_alive_t h, int fd)
#endif
{
  F_LOGE(true, true, LC_YELLOW, "Client not alive, closing fd %d", fd);
  httpd_sess_trigger_close (wss_keep_alive_get_user_ctx (h), fd);
  return true;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR bool check_client_alive_cb (wss_keep_alive_t h, int fd)
#else
bool check_client_alive_cb (wss_keep_alive_t h, int fd)
#endif
{
  bool work_queued = false;

  struct async_resp_arg *resp_arg = (async_resp_arg *)pvPortMalloc (sizeof (struct async_resp_arg));
  //F_LOGI (true, true, LC_BRIGHT_CYAN, "Allocated %d bytes of memory at location 0x%08X", sizeof (struct async_resp_arg), resp_arg);

  resp_arg->hd = wss_keep_alive_get_user_ctx (h);
  resp_arg->fd = fd;

  if ( httpd_queue_work (resp_arg->hd, send_ping, resp_arg) == ESP_OK )
  {
    work_queued = true;
  }
  F_LOGV(true, true, LC_BRIGHT_GREEN, "resp_arg->hd = %p, work_queued = %s", resp_arg->hd, work_queued?"True":"False");

  return work_queued;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void wss_server_send_messages(void *data)
#else
void wss_server_send_messages(void *data)
#endif
{
  wss_keep_alive_storage_t *keep_alive_storage = (wss_keep_alive_storage_t *)data;
  bool send_messages = true;

  // Send async message to all connected clients
  while ( send_messages )
  {
    // Have we initiated an AP scan already?
    bool scan_queued   = false;
    // A Buffer to store JSON scan response
    char *apScanResPtr = NULL;
    uint16_t apScanResLen = 0;

    // Delay between updates
    vTaskDelay (1000 / portTICK_PERIOD_MS);

    // timeout: check if PING message needed
    for ( int i = 0; i < keep_alive_storage->max_clients; ++i )
    {
      F_LOGV(true, true, LC_BRIGHT_GREEN, "i = %d, keep_alive_storage->clients[i].type = %d", i, keep_alive_storage->clients[i].type);
      if ( keep_alive_storage->clients[i].type == CLIENT_ACTIVE )
      {
        void *ptr = NULL;

        struct async_resp_arg *resp_arg = (async_resp_arg *)pvPortMalloc(sizeof (struct async_resp_arg));
        if ( resp_arg == NULL )
        {
          F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'resp_arg'", sizeof (struct async_resp_arg));
        }
        else
        {
          resp_arg->hd      = keep_alive_storage->user_ctx;
          resp_arg->fd      = keep_alive_storage->clients[i].fd;
          resp_arg->voidPtr = (void *)&keep_alive_storage->clients[i].user_int;

          //F_LOGI (true, true, LC_BRIGHT_CYAN, "Allocated %d bytes of memory at location 0x%08X", sizeof (struct async_resp_arg), resp_arg);
          F_LOGV (true, true, LC_BRIGHT_GREEN, "Active client (hd=%p fd=%d) -> sending async message", resp_arg->hd, resp_arg->fd);

          // Check what information we are sending
          switch ( (ws_info_type_t)keep_alive_storage->clients[i].ws_info )
          {
            case WS_SCHED:
              //send_status_update(resp_arg, keep_alive_storage->clients[i].ws_info);
              ptr = (void *)send_schedule;
              break;
            case WS_STATUS:
              //send_status_update(resp_arg, keep_alive_storage->clients[i].ws_info);
              ptr = (void *)send_status_update;
              break;
            case WS_TASKS:
              //send_status_update(resp_arg, keep_alive_storage->clients[i].ws_info);
              ptr = (void *)send_tasks_update;
              break;
            case WS_APSCAN:
              // This client wants an AP scan
              if ( !scan_queued )
              {
                scan_queued = true;
                wifi_startScan();
              }
              // Have we checked and received from the scan queue already?
              if ( apScanResPtr == NULL )
              {
                apScanResPtr = ws_scan_results(&apScanResLen);
              }
              // If we have a message, send it to the client
              if ( apScanResLen > 0 )
              {
                send_async_frame(resp_arg, (uint8_t *)apScanResPtr, apScanResLen);
                //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "l) Releasing allocation 0x%08X", (unsigned int)resp_arg);
                vPortFree(resp_arg);
              }
              break;
            case WS_GDATA:
              //send_status_update(resp_arg, keep_alive_storage->clients[i].ws_info);
              ptr = (void *)send_eng_data;
              break;
            case WS_LOGS:
              //send_status_update(resp_arg, keep_alive_storage->clients[i].ws_info);
              ptr = (void *)send_log_messages;
              break;
            case WS_NONE:
            default:
              break;
          }

          // If we had a match, send an asynchronous update
          if ( ptr != NULL )
          {
            if ( httpd_queue_work(resp_arg->hd, (void (*)(void *))(ptr), resp_arg) != ESP_OK )
            {
              F_LOGE(true, true, LC_YELLOW, "httpd_queue_work failed!");
              send_messages = false;
              break;
            }/*
            else
            {
              F_LOGW(true, true, LC_GREEN, "HTTP work queued for delivery");
            }*/
          }
        }
      }
    }
    // Housekeeping
    if ( apScanResPtr )
    {
      //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "m) Releasing allocation 0x%08X", (unsigned int)apScanResPtr);
      vPortFree (apScanResPtr);
      apScanResPtr = NULL;
    }
  }
}

// --------------------------------------------------------------------------
// async send function, which we put into the httpd work queue
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void ws_async_send (void *arg)
#else
static void ws_async_send (void *arg)
#endif
{
  struct async_resp_arg* resp_arg = (async_resp_arg *)arg;

  static const char* data = "Async data";
  httpd_ws_frame_t ws_pkt = {};
  ws_pkt.payload = (uint8_t *)data;
  ws_pkt.len     = strlen (data);
  ws_pkt.type    = HTTPD_WS_TYPE_TEXT;

  httpd_ws_send_frame_async (resp_arg->hd, resp_arg->fd, &ws_pkt);

  //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "n) Releasing allocation 0x%08X", (unsigned int)resp_arg);
  vPortFree (resp_arg);
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static esp_err_t trigger_async_send (httpd_handle_t handle, httpd_req_t *req)
#else
static esp_err_t trigger_async_send (httpd_handle_t handle, httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;

  struct async_resp_arg* resp_arg = (async_resp_arg *)pvPortMalloc(sizeof (struct async_resp_arg));
  if ( resp_arg == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'resp_arg'", sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd (req);
    err = httpd_queue_work (handle, ws_async_send, resp_arg);
  }
  else
  {
    //F_LOGI (true, true, LC_BRIGHT_CYAN, "Allocated %d bytes of memory at location 0x%08X", sizeof (struct async_resp_arg), resp_arg);
  }

  return err;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t ws_handler (httpd_req_t *req)
#else
esp_err_t ws_handler (httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;

  if ( req->method == HTTP_GET )
  {
    F_LOGV(true, true, LC_BRIGHT_GREEN, "handle = %p, fd = %d, uri = %s, user_ctx = %08x", req->handle, httpd_req_to_sockfd (req), req->uri, (uint32_t)req->user_ctx);
    wss_keep_alive_client_set_data((wss_keep_alive_t)httpd_get_global_user_ctx (req->handle), httpd_req_to_sockfd (req), (uint32_t) req->user_ctx);
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt = {};

  /* Set max_len = 0 to get the frame len */
  err = httpd_ws_recv_frame (req, &ws_pkt, 0);
  if ( err != ESP_OK )
  {
    F_LOGE(true, true, LC_YELLOW, "httpd_ws_recv_frame failed to get frame len with %d", err);
  }
  else
  {
    F_LOGV (true, true, LC_BRIGHT_GREEN, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len)
    {
      /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
      uint8_t *buf = (uint8_t *)pvPortMalloc (ws_pkt.len + 1);
      if (buf == NULL)
      {
        //F_LOGE (true, true, LC_YELLOW, "Failed to allocate %d bytes of memory for buf", ws_pkt.len + 1);
        return ESP_ERR_NO_MEM;
      }
      ws_pkt.payload = buf;
      /* Set max_len = ws_pkt.len to get the frame payload */
      err = httpd_ws_recv_frame (req, &ws_pkt, ws_pkt.len);
      if (err != ESP_OK)
      {
        F_LOGE (true, true, LC_YELLOW, "failed with %d", err);
        //F_LOGI (true, true, LC_BRIGHT_MAGENTA, "o) Releasing allocation 0x%08X", (unsigned int)buf);
        vPortFree (buf);
        return err;
      }
      F_LOGV (true, true, LC_BRIGHT_GREEN, "Got packet with message: %s", ws_pkt.payload);
    }

    bool send_frame = false;
    F_LOGV (true, true, LC_BRIGHT_GREEN, "Packet type: %d", ws_pkt.type);
    switch (ws_pkt.type)
    {
      // If it was a PONG, update the keep-alive
      case HTTPD_WS_TYPE_PONG:
        F_LOGV (true, true, LC_BRIGHT_GREEN, "Received PONG message");
        err = wss_keep_alive_client_is_active ((wss_keep_alive_t)httpd_get_global_user_ctx (req->handle), httpd_req_to_sockfd (req));
        break;
      // If it was a TEXT message, just echo it back
      case HTTPD_WS_TYPE_TEXT:
        send_frame = true;
        F_LOGV (true, true, LC_BRIGHT_GREEN, "Received packet with message: %s", ws_pkt.payload);
        if (strcmp ((char *)ws_pkt.payload, "Trigger async") == 0)
        {
          err = trigger_async_send (req->handle, req);
        }
        break;
      // Response PONG packet to peer
      case HTTPD_WS_TYPE_PING:
        send_frame = true;
        F_LOGV (true, true, LC_BRIGHT_GREEN, "Got a WS PING frame, Replying PONG");
        ws_pkt.type = HTTPD_WS_TYPE_PONG;
        break;
      // Response CLOSE packet with no payload to peer
      case HTTPD_WS_TYPE_CLOSE:
        send_frame = true;
        ws_pkt.len = 0;
        ws_pkt.payload = NULL;
        break;
      case HTTPD_WS_TYPE_CONTINUE:
      case HTTPD_WS_TYPE_BINARY:
        break;
    }

    if (send_frame)
    {
      err = httpd_ws_send_frame (req, &ws_pkt);
      if (err != ESP_OK)
      {
        F_LOGE (true, true, LC_YELLOW, "httpd_ws_send_frame failed with %d", err);
      }
    }
    F_LOGV (true, true, LC_BRIGHT_GREEN, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", req->handle, httpd_req_to_sockfd (req), httpd_ws_get_fd_info (req->handle, httpd_req_to_sockfd (req)));
  }
  // Return to sender with our status
  return err;
}

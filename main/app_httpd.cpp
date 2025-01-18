
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdio.h>
#include <pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_task.h>

#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <espfs.h>
#include <espfs_webpages.h>

#include "app_main.h"
#include "app_utils.h"
#include "app_httpd.h"
#include "app_lightcontrol.h"
#include "app_websockets.h"
#include "cgi_lightcontrol.h"
#include "device_control.h"
#include "cgi_config.h"
#include "app_ota.h"
#include "app_utils.h"
#include "app_wifi.h"
#include "app_ota.h"

// --------------------------------------------------------------------------
// heap tracing
// --------------------------------------------------------------------------

//#define TRACE_ON

#if defined (CONFIG_HEAP_TRACING)
#ifdef TRACE_ON
#include "esp_heap_trace.h"
#endif
#endif

// --------------------------------------------------------------------------
//  Prototypes
// --------------------------------------------------------------------------
/*
static void disconnect_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void connect_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
*/
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR static void dynamic_request (struct async_resp_arg *resp_arg, char *buffer, uint16_t buflen);
static void sendEspFs (void *arg);
#else
IRAM_ATTR static void dynamic_request (httpd_req_t *req, char *buffer, uint16_t buflen);
static esp_err_t sendEspFs (httpd_req_t *req);
#endif

espfs_fs_t *fs = NULL;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR bool isValidTokenChar (const char *arg)
#else
bool isValidTokenChar (const char *arg)
#endif
{
  bool chk = false;

  // Numeric of underscore
  if ( (*arg > 0x2f && *arg < 0x3a) || *arg == 0x5f )
  {
    chk = true;
  }
  // Alphabetical (upper or lower)
  else if ( (*arg | 32) > 0x60 && (*arg | 32) < 0x7b )
  {
    chk = true;
  }

  return chk;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR const char *get_content_type_from_ext (bool *cachecontrol, const char *file)
#else
const char *get_content_type_from_ext (bool *cachecontrol, const char *file)
#endif
{
  const char *ext = get_filename_ext (file);
  const char *typePtr = NULL;
  *cachecontrol = true;

  if ( !str_cmp ("js", ext) )
  {
    typePtr = "application/javascript";
  }
  else if ( !str_cmp ("json", ext) )
  {
    *cachecontrol = false;
    typePtr = "application/json";
  }
  else if ( !str_cmp ("html", ext) )
  {
    *cachecontrol = false;
    typePtr = "text/html";
  }
  else if ( !str_cmp ("css", ext) )
  {
    typePtr = "text/css";
  }
  else if ( !str_cmp ("png", ext) )
  {
    typePtr = "image/png";
  }
  else if ( !str_cmp ("gif", ext) )
  {
    typePtr = "image/gif";
  }
  else if ( !str_cmp ("jpg", ext) )
  {
    typePtr = "image/jpeg";
  }
  else if ( !str_cmp ("ico", ext) )
  {
    typePtr = "image/x-icon";
  }
  else if ( !str_cmp ("xml", ext) )
  {
    typePtr = "text/xml";
  }
  else if ( !str_cmp ("pdf", ext) )
  {
    typePtr = "application/x-pdf";
  }
  else if ( !str_cmp ("zip", ext) )
  {
    typePtr = "application/x-zip";
  }
  else if ( !str_cmp ("gz", ext) )
  {
    typePtr = "application/x-gzip";
  }
  else /* Catch all */
  {
    *cachecontrol = false;
    typePtr = "text/plain";
  }

  F_LOGV(true, true, LC_GREY, "%s -> %s", file, typePtr);

  return typePtr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void set_content_type_using_ext (httpd_req_t *req, const char *file)
#else
void set_content_type_using_ext (httpd_req_t *req, const char *file)
#endif
{
  bool cachecontrol;

  httpd_resp_set_hdr (req, "Content-Type", get_content_type_from_ext (&cachecontrol, file));
  if ( cachecontrol )
  {
    httpd_resp_set_hdr (req, "Cache-Control", "max-age=7200, public, must-revalidate");
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define ASYNC_HDR_STR "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n%s\r\n"
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void send_async_header_using_ext (struct async_resp_arg *resp_arg, const char *file)
#else
void send_async_header_using_ext (struct async_resp_arg *resp_arg, const char *file)
#endif
{
  char hdrbuf[HTTPD_MAX_REQ_HDR_LEN+1];
  int  hdrlen = 0;
  bool cachecontrol;
  const char *typePtr = get_content_type_from_ext (&cachecontrol, file);

  //printf("file: %s -> mime: %s\n", file, typePtr);
  if ( cachecontrol )
  {
    hdrlen = sprintf(hdrbuf, ASYNC_HDR_STR, typePtr, "Cache-Control: max-age=7200, public, must-revalidate\r\n");
  }
  else
  {
    hdrlen = sprintf(hdrbuf, ASYNC_HDR_STR, typePtr, "");
  }

  httpd_socket_send (resp_arg->hd, resp_arg->fd, hdrbuf, hdrlen, 0);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static esp_err_t cgiRedirect (httpd_req_t *req)
#else
static esp_err_t cgiRedirect (httpd_req_t *req)
#endif
{
  const char *uri = (const char *)req->user_ctx;

  // Set status
  httpd_resp_set_status (req, "301 Moved Permanently");

  // Redirect to the new uri
  httpd_resp_set_hdr (req, "Location", uri);

  // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
  httpd_resp_send (req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

  F_LOGW(true, true, LC_BRIGHT_YELLOW, "Redirecting \"%s\" to \"%s\"", req->uri, uri);

  return ESP_OK;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#define MAX_TOKEN_LEN   64
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR static  void  dynamic_request (struct async_resp_arg *resp_arg, char *buffer, uint16_t buflen)
#else
IRAM_ATTR static  void  dynamic_request (httpd_req_t *req, char *buffer, uint16_t buflen)
#endif
{
  char tokenStr[MAX_TOKEN_LEN + 1];
  bool inToken = false;
  int  tok_pos = 0;
  int  buf_pos = 0;
  int  pct_pos = 0;
  int i = 0;

  // Process the buffer for tokens to convert
  for ( ; i < buflen; i++ )
  {
    if ( buffer[i] == '%' )
    {
      if ( !inToken )
      {
        inToken = true;
        tok_pos = 0;
        pct_pos = i;
        if ( (i - buf_pos) > 0 )
        {
#if defined (CONFIG_HTTPD_USE_ASYNC)
          httpd_socket_send(resp_arg->hd, resp_arg->fd, &buffer[buf_pos], i - buf_pos, 0);
#else
          httpd_resp_send_chunk(req, &buffer[buf_pos], i - buf_pos);
#endif
        }
        buf_pos = i + 1;
      }
      else
      {
        inToken = false;
        tokenStr[tok_pos] = 0x0;

#if defined (CONFIG_HTTPD_USE_ASYNC)
        esp_err_t err = ((httpCallback)(resp_arg->fp))(resp_arg, tokenStr);
#else
        esp_err_t err = ((httpCallback)(req->user_ctx))(req, tokenStr);
#endif
        if ( err != ESP_OK )
        {
#if defined (CONFIG_HTTPD_USE_ASYNC)
          httpd_socket_send(resp_arg->hd, resp_arg->fd, &buffer[pct_pos], ((i + 1) - pct_pos), 0);
#else
          httpd_resp_send_chunk(req, &buffer[pct_pos], ((i + 1) - pct_pos));
#endif
        }
        buf_pos = i + 1;
      }
    }
    else if ( inToken )
    {
      // Have we exceeded the token length?
      if ( tok_pos >= MAX_TOKEN_LEN )
      {
        F_LOGE(true, true, LC_YELLOW, "Token overrun");
        inToken = false;
      }
      else
      {
        // Is this a valid token character?
        if ( isValidTokenChar(&buffer[i]) )
        {
          tokenStr[tok_pos++] = buffer[i];
        }
        else
        {
          inToken = false;
          buf_pos = pct_pos;
        }
      }
    }
  }

  // Print the remaining buffer
  if ( (buflen - buf_pos) > 0 )
  {
#if defined (CONFIG_HTTPD_USE_ASYNC)
    httpd_socket_send(resp_arg->hd, resp_arg->fd, &buffer[buf_pos], buflen - buf_pos, 0);
#else
    httpd_resp_send_chunk(req, &buffer[buf_pos], buflen - buf_pos);
#endif
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR static  void  sendEspFs (void *arg)
{
  struct async_resp_arg *req = (struct async_resp_arg *)arg;
#else
IRAM_ATTR static  esp_err_t sendEspFs (httpd_req_t * req)
{
  esp_err_t err = ESP_FAIL;
#endif
  espfs_stat_t stat;

  F_LOGD(true, true, LC_BRIGHT_MAGENTA, "sendEspFs() %s\n", req->uri);

  if ( fs == NULL )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "espfs not initialised");
  }
  else if ( espfs_stat(fs, req->uri, &stat) )
  {
    if ( stat.type == ESPFS_TYPE_FILE )
    {
      espfs_file_t *f = espfs_fopen(fs, req->uri);
      if ( f == NULL )
      {
        F_LOGE(true, true, LC_YELLOW, "Error opening file: '%s'", req->uri);
      }
      else if ( stat.size == 0 )
      {
        F_LOGE(true, true, LC_YELLOW, "stat.size reported 0 bytes for '%s'", req->uri);
        espfs_fclose(f);
      }
      else
      {
        F_LOGD (true, true, LC_GREY, "Preparing %s (%d bytes), sys free: %d bytes, task free: %d", req->uri, stat.size, esp_get_free_heap_size(), uxTaskGetStackHighWaterMark(NULL));
        // Try an allocate some work space
        char *tmpBuffer = (char *)pvPortMalloc (stat.size);
        if ( tmpBuffer == NULL )
        {
          F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating 'tmpBuffer' (%d bytes)", stat.size);
        }
        else
        {
          //F_LOGI (true, true, LC_BRIGHT_CYAN, "y) Allocated %d bytes of memory starting at: 0x%08X", stat.size, (unsigned int)tmpBuffer);

          int bytes = espfs_fread (f, tmpBuffer, stat.size);
          espfs_fclose(f);

#if defined (CONFIG_HTTPD_USE_ASYNC)
          // Set our content type
          send_async_header_using_ext (req, req->uri);
#else
          set_content_type_using_ext (req, req->uri);
#endif

          // Check if we have a function to process the request
#if defined (CONFIG_HTTPD_USE_ASYNC)
          if ( req->fp )
#else
          if ( req->user_ctx )
#endif
          {
            dynamic_request(req, tmpBuffer, bytes);
          }
          // Else, just send the file...
          else
          {
            // After sending the appropriate header....
#if defined (CONFIG_HTTPD_USE_ASYNC)
            httpd_socket_send(req->hd, req->fd, tmpBuffer, bytes, 0);
#else
            httpd_resp_send_chunk(req, tmpBuffer, bytes);
#endif
          }

#if defined (CONFIG_HTTPD_USE_ASYNC)
          // Terminate the connection
          httpd_sess_trigger_close(req->hd, req->fd);
#else
          // Signal completion
          httpd_resp_send_chunk(req, NULL, 0);
#endif

          // Free our work space
          //F_LOGI (true, true, LC_MAGENTA, "a) Releasing allocation 0x%08X", (unsigned int)tmpBuffer);
          vPortFree (tmpBuffer);
          tmpBuffer = NULL;
        }
      }

#ifndef CONFIG_HTTPD_USE_ASYNC
      // Alls okay
      err = ESP_OK;
#endif
    }
    else if ( stat.type == ESPFS_TYPE_DIR )
    {
      // Do something sensible...
    }
    else
    {
      F_LOGE(true, true, LC_YELLOW, "Object \"%s\" is an unknown type", req->uri);
    }
  }

  // Clean up our mess
#if defined (CONFIG_HTTPD_USE_ASYNC)
  //F_LOGI (true, true, LC_MAGENTA, "b) Releasing allocation 0x%08X", (unsigned int)req->uri);
  vPortFree (req->uri);
  //F_LOGI (true, true, LC_MAGENTA, "c) Releasing allocation 0x%08X", (unsigned int)arg);
  vPortFree(arg);
#else
  return err;
#endif
}
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR static  esp_err_t  async_espfs_handler (httpd_req_t * req)
{
  struct async_resp_arg *resp_arg = (async_resp_arg *)pvPortMalloc (sizeof (struct async_resp_arg));
  if (resp_arg == NULL)
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'resp_arg'", sizeof(struct async_resp_arg));
  }
  else
  {
    //F_LOGI (true, true, LC_BRIGHT_CYAN, "x) Allocated %d bytes of memory at location 0x%08X", sizeof (struct async_resp_arg), resp_arg);

    char decUri[512] = {};
    url_decode (decUri, req->uri, 512);
    //F_LOGV(true, true, LC_GREY, "cgiConfig: %s -> %s\n", req->uri, decUri);

    resp_arg->uri = strdup (decUri);
    resp_arg->fd  = httpd_req_to_sockfd (req);
    resp_arg->hd  = req->handle;
    resp_arg->fp  = req->user_ctx;

    //F_LOGV(true, true, LC_MAGENTA, "%0X -> %0X *\n", (unsigned int)&resp_arg->fp, (unsigned int)resp_arg->fp);

    if ( resp_arg->fd < 0 )
    {
      return ESP_FAIL;
    }

    httpd_queue_work (req->handle, sendEspFs, resp_arg);
  }
  return ESP_OK;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static void  async_cgi_resp (void *arg)
#else
static void  async_cgi_resp (void *arg)
#endif
{
  struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;

  if ( resp_arg->fp )
  {
    F_LOGV(true, true, LC_GREY, "%s", resp_arg->uri);
    esp_err_t err = ((cgiCallback)(resp_arg->fp))(resp_arg);
    if ( err )
    {
      F_LOGE(true, true, LC_YELLOW, "%s, err = %d", resp_arg->uri, err);
    }
  }
}

// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static esp_err_t  async_cgi_handler (httpd_req_t *req)
#else
static esp_err_t  async_cgi_handler (httpd_req_t *req)
#endif
{
  struct async_resp_arg *resp_arg = (async_resp_arg *)pvPortMalloc (sizeof (struct async_resp_arg));

  F_LOGD (true, true, LC_GREY, "Preparing %s. sys free: %d bytes, task free: %d", req->uri, esp_get_free_heap_size (), uxTaskGetStackHighWaterMark (NULL));

  if (resp_arg == NULL)
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'resp_arg'", sizeof(struct async_resp_arg));
  }
  else
  {
    //F_LOGI (true, true, LC_BRIGHT_CYAN, "z) Allocated %d bytes of memory at location 0x%08X", sizeof (struct async_resp_arg), resp_arg);

    char  decUri[512] = {};
    url_decode (decUri, req->uri, 512);

    resp_arg->uri = strdup (decUri);
    resp_arg->fd  = httpd_req_to_sockfd (req);
    resp_arg->hd  = req->handle;
    resp_arg->fp  = req->user_ctx;

    if ( resp_arg->fd < 0 )
    {
      return ESP_FAIL;
    }

    httpd_queue_work (req->handle, async_cgi_resp, resp_arg);
  }
  return ESP_OK;
}
#endif

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
static const httpd_uri_t basic_handlers[] = {
#if defined (CONFIG_HTTPD_USE_ASYNC)
    {.uri = "/index.html",          .method = HTTP_GET,     .handler = async_espfs_handler,   .user_ctx = (void *)tplDeviceConfig,      .is_websocket = false, },
    {.uri = "/mqttconfig.html",     .method = HTTP_GET,     .handler = async_espfs_handler,   .user_ctx = (void *)tplMqttConfig,        .is_websocket = false, },
    {.uri = "/led.cgi",             .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiLed,               .is_websocket = false, },
    {.uri = "/pattern.json",        .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiPatterns,          .is_websocket = false, },
    {.uri = "/theme.json",          .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiThemes,            .is_websocket = false, },
    {.uri = "/schedule.json",       .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiSchedule,          .is_websocket = false, },

    {.uri = "/flash/info",          .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiGetFlashInfo,      .is_websocket = false, },
    {.uri = "/flash/reboot",        .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiRebootFirmware,    .is_websocket = false, },
    {.uri = "/flash/setboot",       .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiSetBoot,           .is_websocket = false, },
    {.uri = "/flash/upload",        .method = HTTP_POST,    .handler = cgiUploadFirmware,     .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/flash/erase",         .method = HTTP_GET,     .handler = async_cgi_handler,     .user_ctx = (void *)cgiEraseFlash,        .is_websocket = false, },

    {.uri = "/wifi.html",           .method = HTTP_GET,     .handler = async_espfs_handler,   .user_ctx = (void *)tplDeviceConfig,      .is_websocket = false, },
    {.uri = "/wifi/cfgsta",         .method = HTTP_POST,    .handler = cgiStaCfg,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/wifi/setap",          .method = HTTP_POST,    .handler = cgiWifiSetAp,          .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/wifi/setmode",        .method = HTTP_POST,    .handler = cgiWifiSetMode,        .user_ctx = NULL,                         .is_websocket = false, },

    {.uri = "/config.cgi",          .method = HTTP_GET,     .handler = cgiConfig,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/websocket/schedule",  .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_SCHED,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/status",    .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_STATUS,        .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/tasks",     .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_TASKS,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/apscan",    .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_APSCAN,        .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/gdata",     .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_GDATA,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/log",       .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_LOGS,          .is_websocket = true,  .handle_ws_control_frames = true },

    {.uri = "/",                    .method = HTTP_GET,     .handler = cgiRedirect,           .user_ctx = (char *)"/index.html",        .is_websocket = false, },
    {.uri = "/*",                   .method = HTTP_GET,     .handler = async_espfs_handler,   .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/*",                   .method = HTTP_POST,    .handler = async_espfs_handler,   .user_ctx = NULL,                         .is_websocket = false, },
#else
    {.uri = "/index.html",          .method = HTTP_GET,     .handler = sendEspFs,             .user_ctx = (void *)tplDeviceConfig,      .is_websocket = false, },
    {.uri = "/mqttconfig.html",     .method = HTTP_GET,     .handler = sendEspFs,             .user_ctx = (void *)tplMqttConfig,        .is_websocket = false, },
    {.uri = "/led.cgi",             .method = HTTP_GET,     .handler = cgiLed,                .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/pattern.json",        .method = HTTP_GET,     .handler = cgiPatterns,           .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/theme.json",          .method = HTTP_GET,     .handler = cgiThemes,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/schedule.json",       .method = HTTP_GET,     .handler = cgiSchedule,           .user_ctx = NULL,                         .is_websocket = false, },

    {.uri = "/flash/info",          .method = HTTP_GET,     .handler = cgiGetFlashInfo,       .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/flash/reboot",        .method = HTTP_GET,     .handler = cgiRebootFirmware,     .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/flash/setboot",       .method = HTTP_GET,     .handler = cgiSetBoot,            .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/flash/upload",        .method = HTTP_POST,    .handler = cgiUploadFirmware,     .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/flash/erase",         .method = HTTP_GET,     .handler = cgiEraseFlash,         .user_ctx = NULL,                         .is_websocket = false, },

    {.uri = "/wifi.html",           .method = HTTP_GET,     .handler = sendEspFs,             .user_ctx = (void *)tplDeviceConfig,      .is_websocket = false, },
    {.uri = "/wifi/cfgsta",         .method = HTTP_POST,    .handler = cgiStaCfg,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/wifi/setap",          .method = HTTP_POST,    .handler = cgiWifiSetAp,          .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/wifi/setmode",        .method = HTTP_POST,    .handler = cgiWifiSetMode,        .user_ctx = NULL,                         .is_websocket = false, },

    {.uri = "/config.cgi",          .method = HTTP_GET,     .handler = cgiConfig,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/websocket/schedule",  .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_SCHED,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/status",    .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_STATUS,        .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/tasks",     .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_TASKS,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/apscan",    .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_APSCAN,        .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/gdata",     .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_GDATA,         .is_websocket = true,  .handle_ws_control_frames = true },
    {.uri = "/websocket/log",       .method = HTTP_GET,     .handler = ws_handler,            .user_ctx = (uint32_t *)WS_LOGS,          .is_websocket = true,  .handle_ws_control_frames = true },

    {.uri = "/",                    .method = HTTP_GET,     .handler = cgiRedirect,           .user_ctx = (char *)"/index.html",        .is_websocket = false, },
    {.uri = "/*",                   .method = HTTP_GET,     .handler = sendEspFs,             .user_ctx = NULL,                         .is_websocket = false, },
    {.uri = "/*",                   .method = HTTP_POST,    .handler = sendEspFs,             .user_ctx = NULL,                         .is_websocket = false, }
#endif
};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour
static const int basic_handlers_no = sizeof (basic_handlers) / sizeof (httpd_uri_t);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static void register_basic_handlers (httpd_handle_t server)
{
  esp_err_t err = ESP_FAIL;
  int i;

  F_LOGI(true, true, LC_GREY, "Registering basic handlers");
  F_LOGI(true, true, LC_GREY, "No of handlers = %d", basic_handlers_no);

  for ( i = 0; i < basic_handlers_no; i++ )
  {
    F_LOGV(true, true, LC_GREY, "Registering URI Handler for: \"%s\"", basic_handlers[i].uri);
    err = httpd_register_uri_handler (server, &basic_handlers[i]);
    if ( err != ESP_OK )
    {
      F_LOGE(true, true, LC_YELLOW, "register uri failed for %d", i);
      break;
    }
  }
}

/*
// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
IRAM_ATTR static void connect_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  httpd_handle_t* server = ( httpd_handle_t* )arg;

  F_LOGV(true, true, LC_GREY, "connect_handler() %0X -> %0X", ( unsigned int )server, ( unsigned int )*server);
  if ( *server == NULL )
  {
    *server = start_webserver ();
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
IRAM_ATTR static void disconnect_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  httpd_handle_t* server = ( httpd_handle_t* )arg;

  if ( *server )
  {
    stop_webserver (*server);
    *server = NULL;
  }
}
*/

// --------------------------------------------------------------------------
// Check if ESPFS+ is initialised and return ESP_OK if available
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t check_espfs (void)
#else
esp_err_t check_espfs (void)
#endif
{
  // Check if ESPFS+ is initialiased
  if ( fs == NULL )
  {
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
    // No, so lets try to initialise it
    espfs_config_t espfs_config = {
      .addr = webpages_espfs_start,
    };
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

    fs = espfs_init(&espfs_config);
  }

  return (fs == NULL?ESP_FAIL:ESP_OK);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
esp_err_t init_http_server (httpd_handle_t * httpServer)
{
  esp_err_t err = ESP_FAIL;

  // ESPFS+ initialised now?
  if ( check_espfs () == ESP_FAIL )
  {
    F_LOGE(true, true, LC_YELLOW, "espfs_init() failed");
  }

  return err;
}

// --------------------------------------------------------------------------
// Catch all error handler
// --------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t http_error_handler (httpd_req_t *req, httpd_err_code_t err)
#else
esp_err_t http_error_handler (httpd_req_t *req, httpd_err_code_t err)
#endif
{
  F_LOGE(true, true, LC_YELLOW, "Failed to handle URI: %s", req->uri);

  // Set status
  httpd_resp_set_status (req, "302 Temporary Redirect");

  // Redirect to the "/" root directory
  httpd_resp_set_hdr (req, "Location", "/index.html");

  // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
  httpd_resp_send (req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

  F_LOGI(true, true, LC_GREY, "Redirecting to root");
  return ESP_OK;
}

esp_err_t httpd_open (httpd_handle_t hd, int sockfd)
{
  F_LOGV(true, true, LC_BRIGHT_GREEN, "Handle hd=%p: opening websocket fd=%d", hd, sockfd);
  wss_keep_alive_t h = (wss_keep_alive_t)httpd_get_global_user_ctx(hd);
  return wss_keep_alive_add_client(h, sockfd);
}

void httpd_close (httpd_handle_t hd, int sockfd)
{
  F_LOGV(true, true, LC_BRIGHT_GREEN, "Handle hd=%p: closing websocket fd=%d", hd, sockfd);

  wss_keep_alive_remove_client((wss_keep_alive_t)httpd_get_global_user_ctx(hd), sockfd);
  close (sockfd);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
httpd_handle_t start_webserver (void)
{
  httpd_handle_t server = NULL;

  // Prepare keep-alive engine
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable the warning you're getting
  wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT ();
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

  keep_alive_config.max_clients           = MAX_HTTPD_CLIENTS;
  keep_alive_config.client_not_alive_cb   = client_not_alive_cb;
  keep_alive_config.check_client_alive_cb = check_client_alive_cb;
  wss_keep_alive_t keep_alive             = wss_keep_alive_start(&keep_alive_config);

  // Configure our webserver
  httpd_config_t config                   = HTTPD_DEFAULT_CONFIG ();
  config.max_open_sockets                 = MAX_HTTPD_CLIENTS;
  config.core_id                          = tskNO_AFFINITY;  // tskNO_AFFINITY, 0 = PRO_CPU Task, 1 = APP_CPU Task
  config.uri_match_fn                     = httpd_uri_match_wildcard;
  config.task_priority                    = ESP_TASK_PRIO_MIN + 1;
  config.stack_size                       = STACKSIZE_HTTPD;
  config.lru_purge_enable                 = true;
  config.lru_purge_enable                 = true;
  config.backlog_conn                     = 15;
  config.max_uri_handlers                 = basic_handlers_no;
  config.server_port                      = 80;
  config.open_fn                          = httpd_open;
  config.close_fn                         = httpd_close;
  config.global_user_ctx                  = keep_alive;

/*
  // Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
  // and re-start it upon connection.
#if defined (CONFIG_ESP32_WIFI_ENABLED)
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_ESP32_WIFI_ENABLED
#if defined (CONFIG_ETH_ENABLED)
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_ETH_ENABLED
*/

  // Attempt to start the webserver
  esp_err_t err = httpd_start(&server, &config);
  if ( err == ESP_OK )
  {
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Started HTTP server on port", config.server_port);
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Max URI handlers", config.max_uri_handlers);
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Max Open Sessions", config.max_open_sockets);
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Max Header Length", HTTPD_MAX_REQ_HDR_LEN);
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Max URI Length", HTTPD_MAX_URI_LEN);
    F_LOGI(true, true, LC_GREY, "%27s: %d", "Max Stack Size", config.stack_size);

    register_basic_handlers(server);
    wss_keep_alive_set_user_ctx(keep_alive, server);
    httpd_register_err_handler(server, HTTPD_500_INTERNAL_SERVER_ERROR, http_error_handler);
  }
  else
  {
    F_LOGE(true, true, LC_YELLOW, "HTTP server failed to start, %s", esp_err_to_name (err));
  }

  return server;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void stop_webserver (httpd_handle_t server)
{
  if ( server )
  {
    F_LOGI(true, true, LC_GREY, "Stopping webserver");
    httpd_stop(server);
    server = NULL;
  }
}

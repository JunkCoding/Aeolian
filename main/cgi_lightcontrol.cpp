


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_server.h>
#include <espfs_webpages.h>

#include "cgi_lightcontrol.h"
#include "app_lightcontrol.h"
#include "app_schedule.h"
#include "app_main.h"
#include "app_utils.h"
#include "app_yuarel.h"

#define BUF_SIZE        4096
#define TMP_BUFSIZE     128
#define MAX_ROWS        5

extern themes_t themes[];

// Cgi that turns the security light on or off according to the 'led' param in the POST data
#if defined (CONFIG_HTTPD_USE_ASYNC)
//IRAM_ATTR esp_err_t cgiLed (struct async_resp_arg* resp_arg)
esp_err_t cgiLed (struct async_resp_arg* resp_arg)
{
  struct async_resp_arg* req = (struct async_resp_arg*)resp_arg;
#else
//IRAM_ATTR esp_err_t cgiLed (httpd_req_t * req)
esp_err_t cgiLed (httpd_req_t * req)
{
#endif
  esp_err_t err = ESP_FAIL;
  struct    yuarel url;
  struct    yuarel_param params[MAX_URI_PARTS];
  int       pc = 0;

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode (decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse(&url, decUri) )
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url!");
  }
  else if ((pc = yuarel_parse_query (url.query, '&', params, MAX_URI_PARTS)))
  {
    const char* ptr = uri_arg (params, pc, "led");
    if (ptr != NULL)
    {
      set_light (SECURITY_LIGHT, atoi (ptr), 40);
    }

#if defined (CONFIG_HTTPD_USE_ASYNC)
    httpd_sess_trigger_close (req->hd, req->fd);
#else
    httpd_resp_send (req, NULL, 0);
#endif

    err = ESP_OK;
  }

  return err;
}

#if defined (CONFIG_HTTPD_USE_ASYNC)
//IRAM_ATTR esp_err_t cgiThemes(struct async_resp_arg* resp_arg)
esp_err_t cgiThemes(struct async_resp_arg* resp_arg)
{
  struct async_resp_arg* req = (struct async_resp_arg*)resp_arg;
#else
//IRAM_ATTR esp_err_t cgiThemes(httpd_req_t * req)
esp_err_t cgiThemes(httpd_req_t * req)
{
#endif
  esp_err_t err = ESP_FAIL;
  struct   yuarel url;
  struct   yuarel_param params[MAX_URI_PARTS];
  char     tmpbuf[BUF_SIZE + 1];
  bool     brief = false;
  uint16_t bufptr = 0;
  int      pos = 0;
  int      pc = 0;
  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode(decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if (-1 == yuarel_parse (&url, decUri))
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url!");
  }
  else if ((pc = yuarel_parse_query (url.query, '&', params, MAX_URI_PARTS)))
  {
    // Terse response?
    const char* ptr = uri_arg(params, pc, "terse");
    if (ptr != NULL)
    {
      brief = true;
    }

    // Start position requested?
    ptr = uri_arg(params, pc, "start");
    if (ptr != NULL)
    {
      pos = atoi (ptr);

      // Valid start position?
      if (pos < 0 || pos >= control_vars.num_themes)
      {
        pos = 0;
      }
    }

    tmpbuf[bufptr++] = '{';

    // Is the current active item in this grouping?
    if (control_vars.cur_themeID >= pos && pos < (pos + MAX_ROWS))
    {
      bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "\"active\":\"%d\",", control_vars.cur_themeID);
    }

    // More to follow?
    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "\"next\":\"%d\",\"items\":[", (pos + MAX_ROWS) >= control_vars.num_themes?0:(pos + MAX_ROWS));

    // Iterate through all themes
    for (uint8_t x = 0; (pos < control_vars.num_themes) && (x < MAX_ROWS); x++, pos++)
    {
      if ( brief == true )
      {
        bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"id\": %d, \"name\":\"%s\"},", themes[pos].themeIdentifier, themes[pos].name);
      }
      else
      {
        bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"id\": %d, \"name\":\"%s\",\"items\":[", x, themes[pos].name);
        for (uint8_t y = 0; y < themes[pos].count; y++)
        {
          CRGBPalette16 pal = themes[pos].list[y];
          bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"%d\":[", y);
          for (uint8_t z = 0; z < 16; z++)
          {
            cRGB col = (cRGB)pal.entries[z];
            bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "\"#%02X%02X%02X\",", col.r, col.g, col.b);
          }
          bufptr--;  // Remove trailing ','
          bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "]},");
        }
        bufptr--;  // Remove trailing ','
        bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "]},");
      }
    }
    bufptr--;    // Remove trailing ','
    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "]}");
    tmpbuf[bufptr] = 0x0;

#if defined (CONFIG_HTTPD_USE_ASYNC)
    send_async_header_using_ext (req, "/config.json");
    httpd_socket_send (req->hd, req->fd, tmpbuf, bufptr, 0);
    httpd_sess_trigger_close (req->hd, req->fd);
#else
    httpd_resp_set_hdr (req, "Content-Type", "application/json");
    httpd_resp_send_chunk (req, tmpbuf, bufptr);
    httpd_resp_send_chunk (req, NULL, 0);
#endif
    err = ESP_OK;
  }

  return err;
}

#if defined (CONFIG_HTTPD_USE_ASYNC)
//IRAM_ATTR esp_err_t cgiSchedule (struct async_resp_arg* resp_arg)
esp_err_t cgiSchedule (struct async_resp_arg* resp_arg)
{
  struct async_resp_arg* req = (struct async_resp_arg*)resp_arg;
#else
//IRAM_ATTR esp_err_t cgiSchedule (httpd_req_t * req)
esp_err_t cgiSchedule (httpd_req_t * req)
{
#endif
  esp_err_t err = ESP_FAIL;
  struct   yuarel url;
  struct   yuarel_param params[MAX_URI_PARTS];
  char     tmpbuf[BUF_SIZE + 1];
  uint16_t bufptr = 0;
  int      pos = 0;
  int      pc = 0;
  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode (decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if (-1 == yuarel_parse (&url, decUri))
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url!");
  }
  else if ((pc = yuarel_parse_query (url.query, '&', params, MAX_URI_PARTS)))
  {
    // Start position requested?
    const char* ptr = uri_arg (params, pc, "start");
    if (ptr != NULL)
    {
      pos = atoi (ptr);

      // Valid start position?
      if (pos < 0 || pos >= control_vars.num_themes)
      {
        pos = 0;
      }
    }

    // Begin our response
    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"weekly\":[");

    // Iterate through all themes
    for (uint8_t x = 0; (pos < _get_num_w_events()) && (x < MAX_ROWS); x++, pos++)
    {
      bufptr += _get_weekly_event (&tmpbuf[bufptr], (BUF_SIZE - bufptr), x);
      tmpbuf[bufptr++] = ',';
    }
    bufptr--;    // Remove trailing ','
    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "]}");
    tmpbuf[bufptr] = 0x0;

#if defined (CONFIG_HTTPD_USE_ASYNC)
    send_async_header_using_ext (req, "/config.json");
    httpd_socket_send (req->hd, req->fd, tmpbuf, bufptr, 0);
    httpd_sess_trigger_close (req->hd, req->fd);
#else
    httpd_resp_set_hdr (req, "Content-Type", "application/json");
    httpd_resp_send_chunk (req, tmpbuf, bufptr);
    httpd_resp_send_chunk (req, NULL, 0);
#endif
    err = ESP_OK;
  }

  return err;
}

#if defined (CONFIG_HTTPD_USE_ASYNC)
//IRAM_ATTR esp_err_t cgiPatterns (struct async_resp_arg* resp_arg)
esp_err_t cgiPatterns (struct async_resp_arg* resp_arg)
{
  struct async_resp_arg* req = (struct async_resp_arg*)resp_arg;
#else
//IRAM_ATTR esp_err_t cgiPatterns (httpd_req_t * req)
esp_err_t cgiPatterns (httpd_req_t * req)
{
#endif
  esp_err_t err = ESP_FAIL;
  struct   yuarel url;
  struct   yuarel_param params[MAX_URI_PARTS];
  char     tmpbuf[BUF_SIZE + 1];
  bool     brief = false;
  uint16_t bufptr = 0;
  uint16_t sp = 0;  // Start pos
  uint16_t cp = 0;  // Current pos
  int      pc = 0;

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode (decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if (-1 == yuarel_parse (&url, decUri))
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url!");
  }
  else if ((pc = yuarel_parse_query (url.query, '&', params, MAX_URI_PARTS)))
  {
    // Terse response?
    const char* ptr = uri_arg (params, pc, "terse");
    if (ptr != NULL)
    {
      brief = true;
    }

    // Start position requested?
    ptr = uri_arg (params, pc, "start");
    if (ptr != NULL)
    {
      sp = atoi (ptr);

      // Valid start position?
      if ( sp > control_vars.num_patterns )
      {
        sp = 0;
      }
    }

    // Skip to rewquested start point
    uint16_t x = 0;
    while ( cp < sp && x <= control_vars.num_patterns )
    {
      if ( brief == true )
      {
        if ( patterns[x].enabled )
        {
          cp++;
        }
      }
      else
      {
        cp++;
      }
      x++;
    }

    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"items\":[");

    while ( cp < (sp + MAX_ROWS) && x <= control_vars.num_patterns )
    {
      if ( brief == true )
      {
        if (patterns[x].enabled)
        {
          cp++;
          bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "{\"id\": %d," " \"name\": \"%s\"},", x, patterns[x].name);
        }
      }
      else
      {
        cp++;
        bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr),
                          "{\"id\": %d,"
                          " \"name\": \"%s\","
                          " \"enabled\": %d,"
                          " \"stars\": %d,"
                          " \"mask\": %d},",
                          x, patterns[x].name, patterns[x].enabled, patterns[x].num_stars, 0);
      }
      x++;
    }
    // Remove trailing ','
    bufptr--;

    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "],\"active\": %d,", control_vars.cur_pattern);
    bufptr += snprintf(&tmpbuf[bufptr], (BUF_SIZE - bufptr), "\"next\": %d}", x > control_vars.num_patterns?0:cp);

    tmpbuf[bufptr] = 0x0;

#if defined (CONFIG_HTTPD_USE_ASYNC)
    send_async_header_using_ext (req, "/config.json");
    httpd_socket_send (req->hd, req->fd, tmpbuf, bufptr, 0);
    httpd_sess_trigger_close (req->hd, req->fd);
#else
    httpd_resp_set_hdr (req, "Content-Type", "application/json");
    httpd_resp_send_chunk (req, tmpbuf, bufptr);
    httpd_resp_send_chunk (req, NULL, 0);
#endif

    err = ESP_OK;
  }

  return err;
}

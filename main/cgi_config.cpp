
// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------



// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_netif.h>
#include <esp_http_server.h>

#include "app_main.h"
#include "app_utils.h"
#include "app_configs.h"
#include "cgi_config.h"
#include "device_control.h"
#include "espfs_webpages.h"
#include "app_yuarel.h"

#define JSON_RSP_BUFSIZE    256

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static const Config_Keyword_t *get_config_keyword(int index)
{
  int num_keywords = 0;
  int offset = 0;

  Configuration_List_t *cfg_list = get_configuration_list();

  for ( int i = 0; i < get_num_configurations (); i++ )
  {
    num_keywords += cfg_list[i]->num_keywords;

    if ( index < num_keywords )
    {
      const Config_Keyword_t *keyword = &cfg_list[i]->config_keywords[index - offset];
      return keyword;
    }

    offset = num_keywords;
  }

  return nullptr;
}

static int get_config (int id, char **str, int *value)
{
  Configuration_List_t *cfg_list = get_configuration_list();

  for ( int i = 0; i < get_num_configurations (); i++ )
  {
    int (*item_config) (int id, char **str, int *value) = cfg_list[i]->get_config;

    if ( item_config != NULL )
    {
      if ( item_config (id, str, value) == true )
      {
        F_LOGV(true, true, LC_BRIGHT_GREEN, "0x%0X", id);
        return true;
      }
    }
  }

  return false;
}

static int compare_config (int id, char *str, int value)
{
  Configuration_List_t *cfg_list = get_configuration_list();

  for ( int i = 0; i < get_num_configurations (); i++ )
  {
    int (*item_config) (int id, char *str, int value) = cfg_list[i]->compare_config;

    if ( item_config != NULL )
    {
      int rc = item_config (id, str, value);

      if ( rc != -1 )
      {
        return rc;
      }
    }
  }

  return -1;
}

static int _update_config (int id, char *str, int value)
{
  Configuration_List_t *cfg_list = get_configuration_list ();

  for ( int i = 0; i < get_num_configurations (); i++ )
  {
    int (*item_config) (int id, char *str, int value) = cfg_list[i]->update_config;

    if ( item_config != NULL )
    {
      int rc = item_config (id, str, value);
      F_LOGV(true, true, LC_BRIGHT_GREEN, "rc = %d", rc);

      if ( rc != -1 )
      {
        return rc;
      }
    }
  }

  return -1;
}

static int apply_config (int id, char *str, int value)
{
  Configuration_List_t *cfg_list = get_configuration_list ();

  for ( int i = 0; i < get_num_configurations (); i++ )
  {
    int (*item_config) (int id, char *str, int value) = cfg_list[i]->apply_config;

    if ( item_config != NULL )
    {
      F_LOGV(true, true, LC_BRIGHT_GREEN, "item_config != NULL");
      int rc = item_config (id, str, value);

      if ( rc != -1 )
      {
        return rc;
      }
    }
  }

  return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static int send_json_response (httpd_req_t *req, Config_Keyword_t *keyword, bool eof)
{
  char jsonResp[JSON_RSP_BUFSIZE + 1];
  char ipbuf[65];
  int  buflen = 0;
  char *str = NULL;
  int  value = 0;

  int id = keyword->id;

  //F_LOGW(true, true, LC_BRIGHT_YELLOW, "id: %d, token: %s", keyword->id, keyword->token);
  if ( get_config (id, &str, &value) )
  {
    if ( keyword->type == TEXT )
    {
      buflen = snprintf (jsonResp, JSON_RSP_BUFSIZE, "{ \"param\" : \"%s\", \"value\" : \"%s\" }", keyword->token, str);
    }
    else if ( keyword->type == NUMARRAY )
    {
      F_LOGE(true, true, LC_YELLOW, "Not Implemented");
    }
    else if ( keyword->type == FLAG )
    {
      buflen = snprintf (jsonResp, JSON_RSP_BUFSIZE, "{ \"param\" : \"%s\", \"value\" : \"%d\" }", keyword->token, value == 0?0:1);
    }
    else if ( keyword->type == IP_ADDR )
    {
      buflen = snprintf (jsonResp, JSON_RSP_BUFSIZE, "{ \"param\" : \"%s\", \"value\" : \"%s\" }", keyword->token, esp_ip4addr_ntoa ((esp_ip4_addr_t *)&value, &ipbuf[0], 64));
    }
    else
    {
      buflen = snprintf (jsonResp, JSON_RSP_BUFSIZE, "{ \"param\" : \"%s\", \"value\" : \"%d\" }", keyword->token, value);
    }
  }
  else
  {
    buflen = snprintf (jsonResp, JSON_RSP_BUFSIZE, "{ \"param\" : \"unknown\", \"value\" : \"undef\" }");
  }

  // Generate response in JSON format
  F_LOGV(true, true, LC_GREY, "Json repsonse: %s", jsonResp);

  httpd_resp_set_hdr (req, "Content-Type", "application/json");
  httpd_resp_send_chunk (req, jsonResp, buflen);
  if ( eof == true )
  {
    httpd_resp_send_chunk (req, NULL, 0);
  }

  return buflen;
}

// --------------------------------------------------------------------------
// get the configuration from the web page
// --------------------------------------------------------------------------
#if defined (CONFIG_APPTRACE_SV_ENABLE)
esp_err_t cgiConfig(httpd_req_t *req)
#else
IRAM_ATTR esp_err_t cgiConfig(httpd_req_t *req)
#endif
{
  Config_Keyword_t keyword = {};
  esp_err_t err = ESP_OK;
  struct   yuarel url;
  struct   yuarel_param params[MAX_URI_PARTS];
  char     alignedStr[128] __attribute__ ((aligned (4))) = {};
  int pc = 0;

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode (decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse (&url, decUri) )
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url!");
  }
  else if ( (pc = yuarel_parse_query (url.query, '&', params, MAX_URI_PARTS)) )
  {
    while ( pc-- > 0 && err == ESP_OK )
    {
      int c = get_command (params[pc].key);

      if ( c > -1 )
      {
        char json_resp[512] = {};
        if ( set_implemented(c) )
        {
          err = tplSetConfig(json_resp, 512, params[pc].key, params[pc].val);
        }
        else
        {
          F_LOGE(true, true, LC_YELLOW, "\"%s\" -> \"%s\" *not* implemented", req->uri, decUri);
        }

        httpd_resp_set_hdr (req, "Content-Type", "application/json");
        httpd_resp_send_chunk (req, json_resp, strlen (json_resp));

        err = ESP_OK;
        break;
      }
      else
      {
        for ( int i = 0; i < get_num_keywords (); i++ )
        {
          memcpy(&keyword, get_config_keyword(i), sizeof(Config_Keyword_t));

          F_LOGV(true, true, LC_BRIGHT_GREEN, "get_config_keyword(%d) = %s", i, keyword.token);

          if ( !strcmp (params[pc].key, keyword.token) )
          {
            int id = keyword.id;
            int type = keyword.type;
            F_LOGV(true, true, LC_GREY, ">  #%d of %d keywords", i, get_num_keywords ());
            F_LOGV(true, true, LC_GREY, ">> 0x%02X: %s %s", id, params[pc].key, params[pc].val);

            // 'value' holds the new value for configlist[id]
            // check if the value has changed
            // if so, save the new value to the flash and update the config_list
            if ( type == TEXT )
            {
              strncpy (alignedStr, params[pc].val, 127);
              if ( 0 == compare_config (id, alignedStr, 0) )
              {
                if ( _update_config (id, alignedStr, 0) == 1 )
                {
                  _config_save_str (id, alignedStr, 0, TEXT);
                }

                // apply the changes to the device, module, ...
                apply_config (id, alignedStr, 0);
              }
            }
            else if ( type == NUMARRAY )
            {
              int num_array[NUMARRAY_SIZE] __attribute__ ((aligned (4)));

              int cnt = str2int_array (params[pc].val, NULL, 0);
              if ( cnt > NUMARRAY_SIZE )
              {
                cnt = NUMARRAY_SIZE;
              }

              str2int_array (params[pc].val, num_array, cnt);

              for ( int i = cnt; i < NUMARRAY_SIZE; i++ )
              {
                num_array[i] = 0;
              }

              if ( 0 == compare_config (id, (char *)num_array, 0) )
              {
                if ( _update_config (id, (char *)num_array, 0) == 1 )
                {
                  _config_save_str (id, (char *)num_array, cnt * sizeof (int), NUMARRAY);
                }

                // apply the changes to the device, module, ...
                apply_config (id, (char *)num_array, 0);
              }
            }
            else
            {
              int val = 0;
              if ( type == NUMBER )
              {
                val = str2int (params[pc].val);
              }
              else if ( type == FLAG )
              {
                val = strcmp ("0", params[pc].val) == 0?0:1;
              }
              else if ( type == IP_ADDR )
              {
                str2ip (params[pc].val, &val);
              }

              F_LOGV(true, true, LC_BRIGHT_GREEN, "compare_config(%d, NULL, %d)", id, val);
              if ( 0 == compare_config (id, NULL, val) )
              {
                F_LOGV(true, true, LC_BRIGHT_GREEN, "needs updating");
                if ( _update_config (id, NULL, val) == 1 )
                {
                  F_LOGV(true, true, LC_BRIGHT_GREEN, ">> _config_save_int(%d, %d, %d)", id, val, type);
                  _config_save_int (id, val, type);
                }

                apply_config (id, NULL, val);
              }
            }

            send_json_response(req, &keyword, false);
            err = ESP_OK;
            break;
          }
        }
        // FIXME: Return something if not found
      }
    }
  }

  // Terminate the session...
  httpd_resp_send_chunk(req, NULL, 0);

  return err;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR esp_err_t tplMqttConfig (struct async_resp_arg *resp_arg, char *token)
#else
IRAM_ATTR esp_err_t tplMqttConfig (httpd_req_t *req, char *token, void **arg)
#endif
{
  esp_err_t err = ESP_FAIL;
  char jsonResp[JSON_RSP_BUFSIZE+1];
  char tbuf[JSON_RSP_BUFSIZE+1];
  int buflen = 0;
  int i;

  if ( token != NULL )
  {
    buflen = JSON_RSP_BUFSIZE;
    if ( (err = getDeviceSetting(&tbuf[0], &buflen, token)) == ESP_OK )
    {
      tbuf[buflen] = 0x0;
#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wformat-overflow"
#pragma GCC diagnostic ignored "-Wformat-truncation" // Fuck off, I know what I am doing
      buflen = snprintf(jsonResp, JSON_RSP_BUFSIZE, "%s", tbuf);
#pragma GCC diagnostic pop
    }
    else
    {
      for ( i = 0; i < get_num_keywords(); i++ )
      {
        Config_Keyword_t keyword;
        memcpy(&keyword, get_config_keyword (i), sizeof (Config_Keyword_t));

        F_LOGV(true, true, LC_GREY, "token: %s, keyword.token: %s, keyword.id: %d, i = %d", token, keyword.token, keyword.id, i);

        if ( strcmp(token, keyword.token) == 0 )
        {
          char *str = NULL;
          int  value = 0;

          F_LOGV(true, true, LC_GREY, "token: %s, keyword.token: %s, keyword.id: %x, i = %d", token, keyword.token, keyword.id, i);

          if ( get_config(keyword.id, &str, &value) )
          {
            if ( keyword.type == TEXT )
            {
              buflen = snprintf(jsonResp, JSON_RSP_BUFSIZE, "%s", str);
            }
            else if ( keyword.type == NUMARRAY )
            {
              F_LOGE(true, true, LC_YELLOW, "tplConfig - NumArray Not Implemented");
            }
            else if ( keyword.type == FLAG )
            {
              buflen = snprintf(jsonResp, JSON_RSP_BUFSIZE, "%s", value == 1?"checked":"");
            }
            else if ( keyword.type == IP_ADDR )
            {
              buflen = snprintf(jsonResp, JSON_RSP_BUFSIZE, "%s", esp_ip4addr_ntoa((esp_ip4_addr_t *)&value, &tbuf[0], 64));
            }
            else
            {
              buflen = snprintf(jsonResp, JSON_RSP_BUFSIZE, "%d", value);
            }
            // We found a match, so should be okay
            err = ESP_OK;
            break;
          }
        }
      }
    }

    // Yeh, make sure we don't end the connection.
    // Why there isn't a seperate function to terminate the connection...
    if ( err == ESP_OK && buflen > 0 )
    {
#if defined (CONFIG_HTTPD_USE_ASYNC)
      httpd_socket_send (resp_arg->hd, resp_arg->fd, jsonResp, buflen, 0);
#else
      httpd_resp_send_chunk (req, jsonResp, buflen);
#endif
    }
  }

  return err;
}

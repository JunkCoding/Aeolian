#ifndef __CGI_CONFIG_H__
#define __CGI_CONFIG_H__

#include <esp_http_server.h>
#include <espfs_webpages.h>
#include "app_httpd.h"

esp_err_t cgiConfig(httpd_req_t *req);
#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t tplMqttConfig(struct async_resp_arg *resp_arg, char *token);
#else
esp_err_t tplMqttConfig(httpd_req_t *req, char *token, void **arg);
#endif

#endif

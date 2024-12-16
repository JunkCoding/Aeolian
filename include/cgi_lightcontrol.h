#ifndef __CGI_LIGHTCONTROL_H__
#define __CGI_LIGHTCONTROL_H__

#include <esp_err.h>
#include <esp_http_server.h>
#include "app_yuarel.h"
#include "app_httpd.h"

typedef enum
{
  SCHED_NONE,
  SCHED_WEEKLY,
  SCHED_ANNUAL
} light_sched_t;

const char *uri_arg (struct yuarel_param params[], int parts, const char *key);

#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t cgiLed(struct async_resp_arg *resp_arg);
esp_err_t cgiPatterns(struct async_resp_arg *resp_arg);
esp_err_t cgiThemes(struct async_resp_arg *resp_arg);
esp_err_t cgiSchedule(struct async_resp_arg *resp_arg);
#else
esp_err_t cgiLed(httpd_req_t *req);
esp_err_t cgiPatterns(httpd_req_t *req);
esp_err_t cgiThemes(httpd_req_t *req);
esp_err_t cgiSchedule(httpd_req_t *req);
#endif


#endif // APP_CGI_H

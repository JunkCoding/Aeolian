#ifndef __APP_HTTPD_H__
#define __APP_HTTPD_H__

#include <esp_err.h>
#include <esp_http_server.h>

struct async_resp_arg
{
  httpd_handle_t  hd;
  char           *uri;
  void           *fp;
  void           *voidPtr;
  int             fd;
};

#if defined (CONFIG_HTTPD_USE_ASYNC)
typedef esp_err_t     (* httpCallback)(struct async_resp_arg *resp_arg, char *token);
typedef esp_err_t     (* cgiCallback)(struct async_resp_arg *resp_arg);
void                  send_async_header_using_ext(struct async_resp_arg *resp_arg, const char *file);
#else
typedef esp_err_t     (* httpCallback)(httpd_req_t *req, char *token);
#endif

void                  set_content_type_from_ext(httpd_req_t *req, const char *file);
esp_err_t             init_http_server(httpd_handle_t *httpServer);
httpd_handle_t        start_webserver(void);
void                  stop_webserver(httpd_handle_t server);

#endif

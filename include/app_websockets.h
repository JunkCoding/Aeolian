#ifndef __APP_WEBSOCKETS_H__
#define __APP_WEBSOCKETS_H__

#define WS_KA_PERIOD           15000                      //
#define WS_KA_DECEASED         30000                      //

#define KEEP_ALIVE_CONFIG_DEFAULT()                \
    {                                              \
    .max_clients          = 10,                    \
    .task_stack_size      = 3072,                  \
    .task_prio            = tskIDLE_PRIORITY+1,    \
    .keep_alive_period_ms = WS_KA_PERIOD,          \
    .not_alive_after_ms   = WS_KA_DECEASED,        \
}

struct wss_keep_alive_storage;
typedef struct wss_keep_alive_storage *wss_keep_alive_t;
typedef bool (*wss_check_client_alive_cb_t)(wss_keep_alive_t h, int fd);
typedef bool (*wss_client_not_alive_cb_t)(wss_keep_alive_t h, int fd);

typedef struct
{
  size_t max_clients;                                      // < max number of clients
  size_t task_stack_size;                                  // < stack size of the created task
  size_t task_prio;                                        // < priority of the created task
  size_t keep_alive_period_ms;                             // < check every client after this time
  size_t not_alive_after_ms;                               // < consider client not alive after this time
  wss_check_client_alive_cb_t check_client_alive_cb;       // < callback function to check if client is alive
  wss_client_not_alive_cb_t   client_not_alive_cb;         // < callback function to notify that the client is not alive
  void *user_ctx;                                          // < user context available in the keep-alive handle
} wss_keep_alive_config_t;

typedef struct wss_keep_alive_storage *wss_keep_alive_t;

typedef enum
{
  WS_NONE,
  WS_STATUS,
  WS_TASKS,
  WS_APSCAN,
  WS_GDATA,
  WS_LOGS,
} ws_info_type_t;

typedef enum
{
  NO_CLIENT = 0,
  CLIENT_FD_ADD,
  CLIENT_FD_REMOVE,
  CLIENT_UPDATE,
  CLIENT_ACTIVE,
  STOP_TASK,
} client_fd_action_type_t;

typedef struct
{
  client_fd_action_type_t     type;
  ws_info_type_t              ws_info;
  int                         fd;
  uint32_t                    user_int;
  uint64_t                    last_seen;
} client_fd_action_t;

typedef struct wss_keep_alive_storage
{
  size_t max_clients;
  wss_check_client_alive_cb_t check_client_alive_cb;
  wss_check_client_alive_cb_t client_not_alive_cb;
  size_t                      keep_alive_period_ms;
  size_t                      not_alive_after_ms;
  void                       *user_ctx;
  QueueHandle_t               q;
  client_fd_action_t          clients[];
} wss_keep_alive_storage_t;

inline void wss_keep_alive_set_user_ctx (wss_keep_alive_t h, void *ctx)
{
  h->user_ctx = ctx;
}

inline void *wss_keep_alive_get_user_ctx (wss_keep_alive_t h)
{
  return h->user_ctx;
}

struct ws_send_cache_t
{
  void     *strptr;
  uint16_t  strlen;
};

wss_keep_alive_t wss_keep_alive_start (wss_keep_alive_config_t *config);
esp_err_t        wss_keep_alive_add_client(wss_keep_alive_t h, int fd);
esp_err_t        wss_keep_alive_remove_client(wss_keep_alive_t h, int fd);
bool             client_not_alive_cb (wss_keep_alive_t h, int fd);
bool             check_client_alive_cb (wss_keep_alive_t h, int fd);
esp_err_t        ws_handler(httpd_req_t *req);
void             wss_server_send_messages(void *data);


#endif

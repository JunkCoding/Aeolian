#ifndef __DEVICE_CONTROL_H__
#define __DEVICE_CONTROL_H__

#include <espfs.h>
#include "app_httpd.h"
#include "app_lightcontrol.h"

#define FILE_CHUNK_LEN 1024

typedef enum
{
  ENCODE_PLAIN = 0,
  ENCODE_HTML,
  ENCODE_JS,
} tEncode;

// Weslh Space Agency Rocket Launch Control Stuff

typedef struct
{
  uint8_t   gpioPin;
  uint8_t   safe;
} rocketList_t;

typedef enum rockets
{
  ROCKET_ONE = 21,
  ROCKET_TWO = 19,
  ROCKET_THREE = 18,
  ROCKET_FOUR = 05,
} rockets_t;
//#define   ROCKET_POWER   21
#define   ROCKET_SWITCHES (1ULL << ROCKET_ONE | 1ULL << ROCKET_TWO | 1ULL << ROCKET_THREE | 1ULL << ROCKET_FOUR)
//#define   ROCKET_SETUP    (1ULL << ROCKET_ONE | 1ULL << ROCKET_TWO | 1ULL << ROCKET_THREE | 1ULL << ROCKET_FOUR | 1ULL << ROCKET_POWER)
#define   ROCKET_SETUP    (1ULL << ROCKET_ONE | 1ULL << ROCKET_TWO | 1ULL << ROCKET_THREE | 1ULL << ROCKET_FOUR)

// Set high/low depending on the setup
#define ROCKET_SAFE       0
#define ROCKET_LAUNCH     1

typedef struct
{
  espfs_fs_t *file;
  void *tplArg;
  char token[64];
  int tokenPos;

  char buff[FILE_CHUNK_LEN + 1];

  bool chunk_resume;
  int buff_len;
  int buff_x;
  int buff_sp;
  char *buff_e;

  tEncode tokEncode;
}
tData;

int         get_command (char *arg);
esp_err_t   getDeviceSetting(char *buf, int *buflen, char *token);
bool        set_implemented (int i);
enum        schedule sched2int (char *schedule);
const       char *int2sched (enum schedule sched);
#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t   tplDeviceConfig (struct async_resp_arg *resp_arg, char *token);
#else
esp_err_t   tplDeviceConfig (httpd_req_t *req, char *token);
#endif
esp_err_t   tplSetConfig (char *json_resp_buf, int resp_buf_size, char *param, char *value);
void        init_device_commands (void);


#endif

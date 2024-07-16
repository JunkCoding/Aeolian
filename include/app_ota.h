#ifndef __APP_OTA_H__
#define __APP_OTA_H__

#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_flash_partitions.h>
#include <esp_http_server.h>
#include <esp_app_format.h>
#include <esp_partition.h>
#include <stdio.h>
#include <ctype.h>

#define OTA_URL_SIZE            256
#define HASH_LEN                32
#define URI_DECODE_BUFLEN       128
#define MAX_HTTPD_CLIENTS       12

typedef enum
{
  OTA_OK = 0,
  OTA_ERR_PARTITION_NOT_FOUND = 1,
  OTA_ERR_PARTITION_NOT_ACTIVATED = 2,
  OTA_ERR_BEGIN_FAILED = 3,
  OTA_ERR_WRITE_FAILED = 4,
  OTA_ERR_END_FAILED = 5,
} TOtaResult;

typedef struct
{
  int type;
  int fw1Pos;
  int fw2Pos;
  int fwSize;
  const char *tagName;
} CgiUploadFlashDef;

int         esp32flashGetUpdateMem(uint32_t *loc, uint32_t *size);
int         esp32flashSetOtaAsCurrentImage();
int         esp32flashRebootIntoOta();
void        init_ota(void);

// Call this function for every line with up to 4 kBytes of hex data.
esp_err_t   otaUpdateWriteHexData ( const char *hexData, int len );
void        otaDumpInformation ();

#if defined (CONFIG_HTTPD_USE_ASYNC)
esp_err_t   cgiEraseFlash ( struct async_resp_arg *req );
esp_err_t   cgiSetBoot ( struct async_resp_arg *req );
esp_err_t   cgiRebootFirmware ( struct async_resp_arg *req );
//esp_err_t   cgiUploadFirmware(struct async_resp_arg *req);
esp_err_t   cgiUploadFirmware ( httpd_req_t *req );
esp_err_t   cgiGetFlashInfo ( struct async_resp_arg *req );
#else
esp_err_t   cgiEraseFlash ( httpd_req_t *req );
esp_err_t   cgiSetBoot ( httpd_req_t *req );
esp_err_t   cgiRebootFirmware ( httpd_req_t *req );
esp_err_t   cgiUploadFirmware ( httpd_req_t *req );
esp_err_t   cgiGetFlashInfo ( httpd_req_t *req );
#endif


#endif  // __APP_OTA_H__

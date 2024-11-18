#ifndef  __APP_FLASH_H__
#define  __APP_FLASH_H__

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
#include <esp_app_desc.h>
#include <esp_flash.h>

#include "app_wifi.h"

typedef enum
{
  TYPE_NONE,
  TYPE_U8,
  TYPE_I8,
  TYPE_U16,
  TYPE_I16,
  TYPE_U32,
  TYPE_I32,
  TYPE_U64,
  TYPE_I64,
  TYPE_STR,
} tNumber;

extern uint32_t boot_count;
extern uint32_t ota_update;

extern esp_app_desc_t       app_info;
extern esp_partition_t     *configured;
extern esp_partition_t     *running;
extern esp_ota_img_states_t ota_state;

void        show_nvs_usage (void);
void        print_app_info (void);
void        printHeapInfo (void);
esp_err_t   delete_nvs_events(const char *ns);
void       *get_nvs_events (const char *ns, uint16_t eventLen, uint16_t *items);
void        save_nvs_event (const char *ns, const char *eventKey, void *eventData, uint16_t dataLen);

const char *ota_state_to_str(esp_ota_img_states_t ota_state);
esp_err_t   save_nvs_str (const char *ns, const char *key, const char *strValue);
esp_err_t   get_nvs_num (const char *location, const char *token, char *value, tNumber type);
esp_err_t   save_nvs_num (const char *location, const char *token, const char *value, tNumber type);

void        get_nvs_led_info (void);
void        boot_init (void);
void        new_fw_delay (void);

#endif
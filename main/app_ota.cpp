

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portable.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include <esp_app_format.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_event.h>
#include <esp_flash.h>
#include <esp_cpu.h>

#include <hal/efuse_ll.h>

#include <nvs_flash.h>

#include <driver/rtc_io.h>

#include <rom/rtc.h>
#include <rom/cache.h>
#include <rom/ets_sys.h>
#include <rom/crc.h>

#include "app_main.h"
#include "app_flash.h"
#include "app_utils.h"
#include "app_httpd.h"
#include "app_ota.h"
#include "app_yuarel.h"
#include "app_lightcontrol.h"

#define ESP_CHIP_REV_ABOVE(rev, min_rev) ((min_rev) <= (rev))
#define ESP_CHIP_REV_MAJOR_AND_ABOVE(rev, min_rev) (((rev) / 100 == (min_rev) / 100) && ((rev) >= (min_rev)))
#define IS_MAX_REV_SET(max_chip_rev_full) (((max_chip_rev_full) != 65535) && ((max_chip_rev_full) != 0))


/*   Size of 32 bytes is friendly to flash encryption */
typedef struct
{
  uint32_t ota_seq;
  uint8_t seq_label[24];
  uint32_t crc;                 /* CRC32 of ota_seq field only */
} ota_select;

static uint32_t ota_select_crc(const ota_select *s)
{
  return crc32_le(UINT32_MAX, ( uint8_t * )&s->ota_seq, 4);
}

static bool ota_select_valid(const ota_select *s)
{
  return s->ota_seq != UINT32_MAX && s->crc == ota_select_crc(s);
}

// ToDo: Allow more OTA partitions than the current 2
static int getOtaSel()
{
  int selectedPart;
  ota_select sa1, sa2;
  const esp_partition_t *otaselpart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  esp_flash_read(( uint32_t )otaselpart->address, ( uint32_t * )&sa1, sizeof(ota_select));
  esp_flash_read(( uint32_t )otaselpart->address + 0x1000, ( uint32_t * )&sa2, sizeof(ota_select));
  if ( ota_select_valid(&sa1) && ota_select_valid(&sa2) )
  {
    selectedPart = (((sa1.ota_seq > sa2.ota_seq) ? sa1.ota_seq : sa2.ota_seq)) % 2;
  }
  else if ( ota_select_valid(&sa1) )
  {
    selectedPart = (sa1.ota_seq) % 2;
  }
  else if ( ota_select_valid(&sa2) )
  {
    selectedPart = (sa2.ota_seq) % 2;
  }
  else
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "esp32 ota: no valid ota select sector found!");
    selectedPart = -1;
  }
  F_LOGI(true, true, LC_BRIGHT_YELLOW, "OTA part select ID: %d", selectedPart);
  return selectedPart;
}

int esp32flashGetUpdateMem(uint32_t *loc, uint32_t *size)
{
  const esp_partition_t *otaselpart;
  int selectedPart = getOtaSel();
  if ( selectedPart == -1 )
    return 0;
  otaselpart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, (esp_partition_subtype_t)(ESP_PARTITION_SUBTYPE_APP_OTA_0 + selectedPart), NULL);
  *loc = otaselpart->address;
  *size = otaselpart->size;
  return 1;
}

int esp32flashSetOtaAsCurrentImage()
{
  const esp_partition_t *otaselpart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  int selSect = -1;
  ota_select sa1, sa2, newsa;
  esp_flash_read(( uint32_t )otaselpart->address, ( uint32_t * )&sa1, sizeof(ota_select));
  esp_flash_read(( uint32_t )otaselpart->address + 0x1000, ( uint32_t * )&sa2, sizeof(ota_select));
  if ( ota_select_valid(&sa1) && ota_select_valid(&sa2) )
  {
    selSect = (sa1.ota_seq > sa2.ota_seq) ? 1 : 0;
  }
  else if ( ota_select_valid(&sa1) )
  {
    selSect = 1;
  }
  else if ( ota_select_valid(&sa2) )
  {
    selSect = 0;
  }
  else
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "esp32 ota: no valid ota select sector found!");
  }
  if ( selSect == 0 )
  {
    newsa.ota_seq = sa2.ota_seq + 1;
    F_LOGI(true, true, LC_BRIGHT_YELLOW, "Writing seq %d to ota select sector 1", newsa.ota_seq);
    newsa.crc = ota_select_crc(&newsa);
    esp_flash_erase_sector(otaselpart->address / 0x1000);
    esp_flash_write(otaselpart->address, ( uint32_t * )&newsa, sizeof(ota_select));
  }
  else
  {
    F_LOGI(true, true, LC_BRIGHT_YELLOW, "Writing seq %d to ota select sector 2", newsa.ota_seq);
    newsa.ota_seq = sa1.ota_seq + 1;
    newsa.crc = ota_select_crc(&newsa);
    esp_flash_erase_sector(otaselpart->address / 0x1000 + 1);
    esp_flash_write(otaselpart->address + 0x1000, ( uint32_t * )&newsa, sizeof(ota_select));
  }
  return 1;
}

int esp32flashRebootIntoOta()
{
  software_reset();
  return 1;
}

// **************************************************************************************************
// * SHA256 of partions
// **************************************************************************************************
static void print_sha256(const uint8_t *image_hash, const char *label)
{
  char hash_print[HASH_LEN * 2 + 1];
  hash_print[HASH_LEN * 2] = 0;
  for ( int i = 0; i < HASH_LEN; ++i )
  {
    sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
  }
  F_LOGI(true, true, LC_GREY, "%16s SHA256: %s", label, hash_print);
}
static void display_sha256(void)
{
  uint8_t sha_256[HASH_LEN] = { 0 };
  esp_partition_t partition;

  // get sha256 digest for the partition table
  partition.address = ESP_PARTITION_TABLE_OFFSET;
  partition.size    = ESP_PARTITION_TABLE_MAX_LEN;
  partition.type    = ESP_PARTITION_TYPE_DATA;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "partition table");

  // get sha256 digest for bootloader
  partition.address = ESP_BOOTLOADER_OFFSET;
  partition.size    = ESP_PARTITION_TABLE_OFFSET;
  partition.type    = ESP_PARTITION_TYPE_APP;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "bootloader");

  // get sha256 digest for running partition
  esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
  print_sha256(sha_256, "firmware");
}

// **************************************************************************************************
  // Validate the current partition after an OTA update
// **************************************************************************************************
IRAM_ATTR void fw_validation_callback(void *arg)
{
  esp_ota_mark_app_valid_cancel_rollback();
  F_LOGI(true, true, LC_YELLOW, "Firmware verified");
}
// **************************************************************************************************
// Check if this partition is flagged as pending verification.
// If so, initiate a timer callback to verify this partition
// after a period of time, long enough to confirm everything
// is working properly.
// **************************************************************************************************
#define FW_VERIFY_DELAY_SECS    60
void init_ota(void)
{
  display_sha256();

  configured = ( esp_partition_t * )esp_ota_get_boot_partition();
  running = ( esp_partition_t * )esp_ota_get_running_partition();

  esp_ota_get_state_partition(running, &ota_state);
  esp_ota_get_partition_description(running, &app_info);

#if defined (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
  if ( ota_state == ESP_OTA_IMG_PENDING_VERIFY )
#else
  if ( ota_state != ESP_OTA_IMG_VALID )
#endif
  {
    F_LOGI(true, true, LC_YELLOW, "Firmware needs verifying...");

    esp_timer_handle_t fwVerifyHandle;
    esp_timer_create_args_t validation_timer_args = {};
    validation_timer_args.callback = &fw_validation_callback;
    validation_timer_args.name = "FW Verification";

    // Create one-shot timer
    // *****************************************
    if ( esp_timer_create(&validation_timer_args, &fwVerifyHandle) != ESP_OK )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "Failed to validation timer");
      return;
    }
    esp_timer_start_once(fwVerifyHandle, FW_VERIFY_DELAY_SECS * 1000000);
  }
}

// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
// * Web server component
// \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

#ifndef UPGRADE_FLAG_FINISH
#define UPGRADE_FLAG_FINISH     0x02
#endif

#define TMP_BUFSIZE             2048
#define PAGELEN                 4096
#define FLST_START              0
#define FLST_WRITE              1
#define FLST_SKIP               2
#define FLST_DONE               3
#define FLST_ERROR              4
#define FILETYPE_ESPFS          0
#define FILETYPE_FLASH          1
#define FILETYPE_OTA            2


static uint32_t update_pointer;
static esp_ota_handle_t update_handle = 0;

const esp_partition_t *get_next_boot_partition();
static void otaDump128bytes(uint32_t addr, uint8_t *p);
static void otaDump16bytes(uint32_t addr, uint8_t *p);

#define   OTA_BUF_SIZE    4096

esp_err_t otaUpdateWriteHexData(const char *hexData, int len)
{
  esp_err_t err = OTA_OK;

  // Erase flash pages at 4k boundaries.
  if ( update_pointer % 0x1000 == 0 )
  {
    int flashSectorToErase = update_pointer / 0x1000;

    esp_flash_erase_sector(flashSectorToErase);
  }

  err = esp_ota_write(update_handle, hexData, len);

  if ( err != ESP_OK )
  {
    return OTA_ERR_WRITE_FAILED;
  }

  update_pointer += len;

  return err;
}

void otaDumpInformation()
{
  esp_err_t result;
  uint8_t *buf = (uint8_t *)pvPortMalloc(OTA_BUF_SIZE);
  if ( buf )
  {
    uint8_t *buf2 = (uint8_t *)pvPortMalloc(OTA_BUF_SIZE);
    if ( buf2 )
    {
      F_LOGI(true, true, LC_GREY, "otaDumpInformation");

      size_t chipSize = esp_flash_get_chip_size();
      F_LOGI(true, true, LC_GREY, "flash chip size = %d", chipSize);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00000000....");
      result = esp_flash_read(0, buf, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0, buf);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00001000....");
      result = esp_flash_read(0x1000, buf, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x1000, buf);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00004000....");
      result = esp_flash_read(0x4000, buf, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x4000, buf);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x0000D000....");
      result = esp_flash_read(0xD000, buf, OTA_BUF_SIZE);
      otaDump128bytes(0xD000, buf);
      result = esp_flash_read(0xE000, buf, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0xE000, buf);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00010000....");
      result = esp_flash_read(0x10000, buf, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x10000, buf);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00020000....");
      result = esp_flash_read(0x20000, buf2, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x20000, buf2);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00110000....");
      result = esp_flash_read(0x110000, buf2, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x110000, buf2);

      F_LOGI(true, true, LC_GREY, "Reading flash at 0x00210000....");
      result = esp_flash_read(0x210000, buf2, OTA_BUF_SIZE);
      F_LOGI(true, true, LC_GREY, "Result = %d", result);
      otaDump128bytes(0x210000, buf2);

      vPortFree(buf2);
      buf2 = NULL;
    }

    vPortFree(buf);
    buf = NULL;
  }
}

const esp_partition_t *get_next_boot_partition()
{
  esp_err_t err;
  const esp_partition_t *update = NULL;

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if ( configured != running )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
    F_LOGE(true, true, LC_BRIGHT_RED, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }

  F_LOGI(true, true, LC_GREY, "Running partition: %02x %02x 0x%08x %s", running->type, running->subtype, running->address, running->label);

  update = esp_ota_get_next_update_partition(NULL);
  assert(update != NULL);
  F_LOGI(true, true, LC_GREY, "   Next partition: %02x %02x 0x%08x %s", update->type, update->subtype, update->address, update->label);

  err = esp_ota_begin(update, OTA_SIZE_UNKNOWN, &update_handle);
  if ( err != ESP_OK )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "esp_ota_begin failed, error=%d", err);
    esp_restart();
  }

  return update;
}

static void otaDump128bytes(uint32_t addr, uint8_t *p)
{
  for ( int i = 0; i < 8; i++ )
  {
    uint32_t addr2 = addr + 16 * i;
    otaDump16bytes(addr2, &p[16 * i]);
  }
}
static void otaDump16bytes(uint32_t addr, uint8_t *p)
{
  F_LOGI(true, true, LC_BRIGHT_YELLOW, "%08X : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
    addr, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}
// ***************************************************************************
// Check that the header of the firmware blob looks like actual firmware...
// ***************************************************************************
// Eg: E9 06 02 3F 30 51 37 40 EE 00 00 00 09 00 00 00
static int checkBinHeader(void *buf)
{
  esp_image_header_t *hdr = (esp_image_header_t *)buf;

  // Check the magic
  // ----------------------------------------
  if ( hdr->magic != ESP_IMAGE_HEADER_MAGIC )
  {
    return 0;
  }

  // Check the target chip ID
  // ----------------------------------------
  if ( CONFIG_IDF_FIRMWARE_CHIP_ID != hdr->chip_id )
  {
    return 0;
  }

  // Check the chip revision
  // ----------------------------------------
  unsigned revision = efuse_hal_chip_revision();
  unsigned int major_rev = revision / 100;
  unsigned int minor_rev = revision % 100;
  unsigned min_rev = hdr->min_chip_rev_full;
  if ( !ESP_CHIP_REV_ABOVE(revision, min_rev) )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Image requires chip rev >= v%d.%d, but chip is v%d.%d", min_rev / 100, min_rev % 100, major_rev, minor_rev);
    return 0;
  }
  unsigned max_rev = hdr->max_chip_rev_full;
  if ((IS_MAX_REV_SET(max_rev) && (revision > max_rev) && !efuse_ll_get_disable_wafer_version_major()))
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Image requires chip rev <= v%d.%d, but chip is v%d.%d", max_rev / 100, max_rev % 100, major_rev, minor_rev);
    return 0;
  }

  return 1;
}
// ---------------------------------------------------------------------
// Check if a partition contains valid app data
// ---------------------------------------------------------------------
int check_partition_valid_app(const esp_partition_t *partition)
{
  if ( partition == NULL )
  {
    return 0;
  }

  esp_image_metadata_t data;
  const esp_partition_pos_t part_pos = {
    .offset = partition->address,
    .size   = partition->size,
  };

  // ESP_IMAGE_VERIFY,       => Verify image contents, Print errors.
  // ESP_IMAGE_VERIFY_SILENT => Verify image contents, Don't print errors.
  if ( esp_image_verify(ESP_IMAGE_VERIFY, &part_pos, &data) != ESP_OK )
  {
    return 0;
  }

  return 1;
}

// ---------------------------------------------------------------------
// Handle request to reboot into the new firmware
// ---------------------------------------------------------------------
IRAM_ATTR static void resetCb(void *arg)
{
  // FIXME: TODO
  //control_vars.reboot = true;
  F_LOGI(true, true, LC_GREY, "calling esp_restart()");
  esp_restart();
}
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR esp_err_t cgiRebootFirmware(struct async_resp_arg *req)
#else
IRAM_ATTR esp_err_t cgiRebootFirmware(httpd_req_t *req)
#endif
{
  const esp_timer_create_args_t reset_timer_args = {
    .callback               = &resetCb,
    .arg                    = NULL,
#ifdef CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
    .dispatch_method        = ESP_TIMER_ISR,
#else
    .dispatch_method        = ESP_TIMER_TASK,
#endif
    .name                   = "reset",
    .skip_unhandled_events  = true
  };

  // Do reboot in a timer callback so we still have time to send the response.
  esp_timer_handle_t reset_timer;
  if ( esp_timer_create(&reset_timer_args, &reset_timer) == ESP_OK )
  {
    esp_timer_start_once(reset_timer, 1500000);

#if defined (CONFIG_HTTPD_USE_ASYNC)
    send_async_header_using_ext(req, "/config.json");
    httpd_socket_send(req->hd, req->fd, JSON_REBOOT_STR, strlen(JSON_REBOOT_STR), 0);
#else
    httpd_resp_set_hdr(req, "Content-Type", "text/json");
    httpd_resp_send_chunk(req, JSON_REBOOT_STR, strlen(JSON_REBOOT_STR));
#endif
  }

#if defined (CONFIG_HTTPD_USE_ASYNC)
  httpd_sess_trigger_close(req->hd, req->fd);
#else
  httpd_resp_send_chunk(req, NULL, 0);
#endif

  return ESP_OK;
}

// ---------------------------------------------------------------------
// Set which partition to boot into on reset
// ---------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR esp_err_t cgiSetBoot(struct async_resp_arg *req)
#else
IRAM_ATTR esp_err_t cgiSetBoot(httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;
  char    tmpbuf[256];
  int     bufptr = 0;
  struct  yuarel url;
  struct  yuarel_param params[MAX_URI_PARTS];
  const char *part = NULL;
  int     pc = 0;

  // Start our JSON response
  bufptr = sprintf(tmpbuf, "{");

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = { 0 };
  url_decode(decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse(&url, decUri) )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Could not parse url!");
  }
  else if ( (pc = yuarel_parse_query(url.query, '&', params, MAX_URI_PARTS)) )
  {
    while ( pc-- > 0 )
    {
      //F_LOGI(true, true, LC_GREY, "%s -> %s", params[pc].key, params[pc].val);
      if ( !str_cmp("partition", params[pc].key) )
      {
        // We found a request to set the boot partition
        part = params[pc].val;

        const esp_partition_t *wanted_bootpart = NULL;
        const esp_partition_t *actual_bootpart = NULL;

        // Find and set the requested partition
        wanted_bootpart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, part);
        if ( wanted_bootpart != NULL )
        {
          err = esp_ota_set_boot_partition(wanted_bootpart);

          // Get the actual boot partition
          actual_bootpart = esp_ota_get_boot_partition();

          bufptr += sprintf(&tmpbuf[bufptr], "\n\t\"boot\": \"%s\",", actual_bootpart->label);

          // Check if we had an error setting the partition
          if ( err != ESP_OK )
          {
            F_LOGE(true, true, LC_BRIGHT_RED, "esp_ota_set_boot_partition() failed! err=0x%x", err);
          }
          else
          {
            //F_LOGI(true, true, LC_GREY, "%0X -> %0X *", (unsigned int)wanted_bootpart, (unsigned int)actual_bootpart);
            if ( wanted_bootpart != actual_bootpart )
            {
              err = ESP_FAIL;
              F_LOGE(true, true, LC_BRIGHT_RED, "wanted_bootpart != actual_bootpart");
            }
            else
            {
              err = ESP_OK;
            }
          }
        }
        break;
      }
    }
  }

  // Update our status
  bufptr += sprintf(&tmpbuf[bufptr], ((ESP_OK == err) ? JSON_SUCCESS_STR : JSON_FAILURE_STR));

#if defined (CONFIG_HTTPD_USE_ASYNC)
  send_async_header_using_ext(req, "/config.json");
  httpd_socket_send(req->hd, req->fd, tmpbuf, bufptr, 0);
  httpd_sess_trigger_close(req->hd, req->fd);
#else
  httpd_resp_set_hdr(req, "Content-Type", "text/json");
  httpd_resp_send_chunk(req, tmpbuf, bufptr);
  httpd_resp_send_chunk(req, NULL, 0);
#endif

  return err;
}

// ---------------------------------------------------------------------
// Erase a partition
// ---------------------------------------------------------------------
#if defined (CONFIG_HTTPD_USE_ASYNC)
//IRAM_ATTR esp_err_t cgiEraseFlash (struct async_resp_arg* req)
esp_err_t cgiEraseFlash(struct async_resp_arg *req)
#else
//IRAM_ATTR esp_err_t cgiEraseFlash (httpd_req_t* req)
esp_err_t cgiEraseFlash(httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;
  const char *part = NULL;
  char    tmpbuf[256];
  int     bufptr = 0;
  int     pc = 0;
  struct  yuarel url;
  struct  yuarel_param params[MAX_URI_PARTS];

  // Start our JSON response
  bufptr = sprintf(tmpbuf, "{");

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = { 0 };
  url_decode(decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse(&url, decUri) )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Could not parse url!");
  }
  else if ( (pc = yuarel_parse_query(url.query, '&', params, MAX_URI_PARTS)) )
  {
    while ( pc-- > 0 )
    {
      //F_LOGI(true, true, LC_GREY, "%s -> %s", params[pc].key, params[pc].val);
      if ( !str_cmp("partition", params[pc].key) )
      {
        part = params[pc].val;

        // Search for the requested partition
        const esp_partition_t *wanted_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, part);

        // Check if we found anything
        if ( wanted_partition )
        {
          // Try and erase it
          if ( ESP_OK == (err = esp_partition_erase_range(wanted_partition, 0, wanted_partition->size)) )
          {
            bufptr += sprintf(&tmpbuf[bufptr], "\n\t\"erased\": \"%s\",", wanted_partition->label);
          }
        }

        break;
      }
    }
  }

  // Update our status
  bufptr += sprintf(&tmpbuf[bufptr], ((ESP_OK == err) ? JSON_SUCCESS_STR : JSON_FAILURE_STR));

#if defined (CONFIG_HTTPD_USE_ASYNC)
  send_async_header_using_ext(req, "/config.json");
  httpd_socket_send(req->hd, req->fd, tmpbuf, bufptr, 0);
  httpd_sess_trigger_close(req->hd, req->fd);
#else
  httpd_resp_set_hdr(req, "Content-Type", "text/json");
  httpd_resp_send_chunk(req, tmpbuf, bufptr);
  httpd_resp_send_chunk(req, NULL, 0);
#endif

  return err;
}

// ---------------------------------------------------------------------
// Handle uploading and writing new firmware
// ---------------------------------------------------------------------
#define RCV_BUFSIZE    2048
//esp_err_t cgiUploadFirmware(struct async_resp_arg *req)
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR esp_err_t cgiUploadFirmware(httpd_req_t *req)
#else
esp_err_t cgiUploadFirmware(httpd_req_t *req)
#endif
{
  esp_err_t err = ESP_FAIL;
  const   esp_partition_t *update_partition;
  const   esp_partition_t *configured;
  const   esp_partition_t *running;
  struct  yuarel url;
  struct  yuarel_param params[MAX_URI_PARTS];
  char *part = NULL;
  int     pc = 0;
  int     bufptr = 0;

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = { 0 };
  url_decode(decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse(&url, decUri) )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Could not parse url!");
  }
  else if ( (pc = yuarel_parse_query(url.query, '&', params, MAX_URI_PARTS)) )
  {
    while ( pc-- > 0 )
    {
      if ( !str_cmp("partition", params[pc].key) )
      {
        part = params[pc].val;
      }
    }
  }

  configured = esp_ota_get_boot_partition();
  running = esp_ota_get_running_partition();

  // check that ota support is enabled
  if ( !configured || !running )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "configured or running parititon is null, is OTA support enabled in build configuration?");
    err = FLST_ERROR;
  }
  else
  {
    if ( part )
    {
      update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, part);
    }
    else
    {
      update_partition = esp_ota_get_next_update_partition(NULL);
    }

    if ( update_partition == NULL )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "update_partition not found!");
      err = FLST_ERROR;
    }
    else
    {
      esp_ota_handle_t update_handle;
      int remaining = req->content_len;
      int received = 0;

      // Stop the lights flickering when writing to flash
      lightsPause(PAUSE_UPLOADING);

      F_LOGI(true, true, LC_GREY, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);
      err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
      if ( ESP_OK != err )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "esp_ota_begin failed, error=%d", err);
        err = FLST_ERROR;
      }
      //FIXME: check firmware size
      else
      {
        // Allocat some storage space to receive the OTA
        char *rcvbuf = (char *)pvPortMalloc(RCV_BUFSIZE);
        if ( rcvbuf != NULL )
        {
          while ( remaining > 0 && ESP_OK == err )
          {
            if ( (received = httpd_req_recv(req, rcvbuf, MIN(remaining, RCV_BUFSIZE))) <= 0 )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "Some kind of error happened, so figure it out");
              err = FLST_ERROR;
            }
            else
            {
              // If first loop (remaining = content_len) check the header, to make sure we
              // are not uploading an invalid image.
              if ( remaining == req->content_len )
              {
                if ( !checkBinHeader(rcvbuf) )
                {
                  F_LOGE(true, true, LC_BRIGHT_RED, "Not a valid firmware image");
                  err = FLST_ERROR;
                }
                otaDump128bytes(( uint32_t )rcvbuf, ( uint8_t * )rcvbuf);
              }

              // If we don't have an error, invalid header for example, we write the bytes
              // to our OTA partition.
              if ( ESP_OK == err )
              {
                err = esp_ota_write(update_handle, rcvbuf, received);
              }

              // Report any errors.
              if ( ESP_OK != err )
              {
                F_LOGE(true, true, LC_BRIGHT_RED, "Error: esp_ota_write failed! err=0x%x", err);
              }
            }

            // Reduce the number of anticipated bytes.
            remaining -= received;
          }
          // Clean up
          vPortFree(rcvbuf);
          rcvbuf = NULL;
        }
        else
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "OTA failed trying to allocate memory for 'rcvbuf'");
        }

        err = esp_ota_end(update_handle);
        if ( ESP_OK != err )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "esp_ota_end failed!");
        }
        else if ( ESP_OK != (err = esp_ota_set_boot_partition(update_partition)) )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "esp_ota_set_boot_partition failed! err=0x%x", err);
        }
        else
        {
          // Save our current reboot number
          // *****************************************
          nvs_handle handle;
          nvs_open(NVS_SYS_INFO, NVS_READWRITE, &handle);
          nvs_set_u32(handle, NVS_OTA_UPDATE_KEY, boot_count);
          nvs_commit(handle);
          nvs_close(handle);
          F_LOGI(true, true, LC_BRIGHT_YELLOW, "OTA update: Complete");
        }
      }
    }
  }

  if ( err )
  {
    F_LOGI(true, true, LC_GREY, "Sending 409 response");
    httpd_resp_set_status(req, "409 OTA Pending Verify");
  }
  else
  {
    F_LOGI(true, true, LC_GREY, "Sending 200 response");
    httpd_resp_set_status(req, "200 OK");
  }

  // Remove our block on the light show
  lightsUnpause(PAUSE_UPLOADING, true);

  char msgbuf[256];
  bufptr += sprintf(&msgbuf[bufptr], "{%s", ((ESP_OK == err) ? JSON_SUCCESS_STR : JSON_FAILURE_STR));
  httpd_resp_send_chunk(req, msgbuf, bufptr);
  httpd_resp_send_chunk(req, NULL, 0);

  // We handled the request, regardless of errors, so we should return ESP_OK
  return ESP_OK;
}


#define PARTITION_IS_FACTORY(partition) (partition->type == ESP_PARTITION_TYPE_APP) && (partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY))
#define PARTITION_IS_OTA(partition) ((partition->type == ESP_PARTITION_TYPE_APP) && (partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN) && (partition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX))
#define APP_FMT_STR "{\n\t\t\t\"name\": \"%s\",\n\t\t\t\"size\": %d,\n\t\t\t\"version\": \"\",\n\t\t\t\"ota\": %s,\n\t\t\t\"running\": %s,\n\t\t\t\"bootset\": %s"
#define DATA_FMT_STR "{\n\t\t\t\"name\": \"%s\",\n\t\t\t\"size\": %d,\n\t\t\t\"format\": %d\n\t\t}"
#if defined (CONFIG_HTTPD_USE_ASYNC)
IRAM_ATTR esp_err_t cgiGetFlashInfo(struct async_resp_arg *req)
#else
IRAM_ATTR esp_err_t cgiGetFlashInfo (httpd_req_t* req)
#endif
{
  esp_err_t err = ESP_OK;
  struct  yuarel       url;
  struct  yuarel_param params[MAX_URI_PARTS];
  bool    show_app   = true;
  bool    show_data  = true;
  bool    verify     = false;
  bool    partname   = false;
  char    *part      = NULL;
  int     pc         = 0;

  // Work with a url decoded version of the original uri string
  char decUri[URI_DECODE_BUFLEN] = {};
  url_decode (decUri, req->uri, URI_DECODE_BUFLEN);

  // Parse the HTTP request
  if ( -1 == yuarel_parse (&url, decUri) )
  {
    F_LOGE(true, true, LC_YELLOW, "Could not parse url! %s", decUri );
  }
  else if ( (pc = yuarel_parse_query(url.query, '&', params, MAX_URI_PARTS)) )
  {
    while ( pc-- > 0 )
    {
      F_LOGV(true, true, LC_BRIGHT_GREEN, "%s -> %s", params[pc].key, params[pc].val);
      // ptype, verify, partition,
      if ( !str_cmp("ptype", params[pc].key) )
      {
        if ( !str_cmp("app", params[pc].val) )
        {
          show_data = false;
        }
        else if ( !str_cmp("data", params[pc].val) )
        {
          show_app = false;
        }
      }
      else if ( !str_cmp("verify", params[pc].key) )
      {
        if ( *params[pc].val == '1' && !*(params[pc].val + 1) )
        {
          verify = true;
        }
      }
      else if ( !str_cmp("partition", params[pc].key) )
      {
        partname = true;
        part = params[pc].val;
      }
    }
  }

  // Fuck installing a second json library to encode this shit
  char jsonStr[TMP_BUFSIZE+1] = {};
  int  strptr = 0;

  // Start our json structure
  strptr = sprintf(jsonStr, "{\n");

  // We showing "application" partitions?
  if ( show_app )
  {
    /* Get partition info */
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
    if ( boot_partition == NULL )
    {
      boot_partition = running_partition;
    }

    strptr += sprintf(&jsonStr[strptr], "\t\"app\": [");

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, (partname) ? (part) : (NULL));
    while ( it != NULL )
    {
      const esp_partition_t *it_partition = esp_partition_get(it);

      if ( it_partition != NULL )
      {
        // Comma seperate JSON objects
        if ( (jsonStr[(strptr - 1)]) == '}' )
        {
          strptr += sprintf(&jsonStr[strptr], ", ");
        }

        // Display the meat of the information
        strptr += sprintf(&jsonStr[strptr], APP_FMT_STR, it_partition->label, it_partition->size,
            (PARTITION_IS_OTA(it_partition) ? "true" : "false"),
            ((it_partition == running_partition) ? "true" : "false"),
            ((it_partition == boot_partition ) ? "true" : "false"));

        // Verify the partition?
        if ( verify )
        {
          strptr += sprintf(&jsonStr[strptr], ",\n\t\t\t\"valid\": %s", (check_partition_valid_app(it_partition) ? "true" : "false"));
        }

        // Finish this JSON object
        strptr += sprintf(&jsonStr[strptr], "\n\t\t}");

        it = esp_partition_next(it);
      }
    }
    esp_partition_iterator_release(it);

    // Done and dusted with "app"
    strptr += sprintf(&jsonStr[strptr], "]");
  }

  if ( show_data )
  {
    // Comma seperate JSON objects
    if ( (jsonStr[(strptr - 1)]) == ']' )
    {
      strptr += sprintf(&jsonStr[strptr], ",\n");
    }

    strptr += sprintf(&jsonStr[strptr], "\t\"data\": [");

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, (partname) ? (part) : (NULL));
    while ( it != NULL )
    {
      const esp_partition_t *it_partition = esp_partition_get(it);

      if ( it_partition != NULL )
      {
        // Comma seperate JSON objects
        if ( (jsonStr[(strptr - 1)]) == '}' )
        {
          strptr += sprintf(&jsonStr[strptr], ", ");
        }

        // Display the meat of the information
        strptr += sprintf(&jsonStr[strptr], DATA_FMT_STR, it_partition->label, it_partition->size, it_partition->subtype);

        it = esp_partition_next(it);
      }
    }
    esp_partition_iterator_release(it);

    // Done and dusted with "app"
    strptr += sprintf(&jsonStr[strptr], "]");
  }

  // The stupidest of *all* stupid validity checks
  strptr += sprintf(&jsonStr[strptr], ",%s", ((jsonStr[(strptr - 1)] != '{') ? JSON_SUCCESS_STR : JSON_FAILURE_STR));

#if defined (CONFIG_HTTPD_USE_ASYNC)
  send_async_header_using_ext(req, "/config.json");
  httpd_socket_send(req->hd, req->fd, jsonStr, strptr, 0);
  httpd_sess_trigger_close(req->hd, req->fd);
#else
  httpd_resp_set_hdr(req, "Content-Type", "application/json");
  httpd_resp_send_chunk(req, jsonStr, strptr);
  httpd_resp_send_chunk(req, NULL, 0);
#endif

  return err;
}

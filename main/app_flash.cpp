



#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_heap_task_info.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>
#include <soc/rmt_struct.h>
#include <soc/rtc_wdt.h>
#include <driver/periph_ctrl.h>
#include <driver/timer.h>
#include <driver/i2s.h>
#include <driver/uart.h>
#include <nvs_flash.h>

// Include this here due to bug of dac.h not including it
#include "driver/gpio.h"

#include <esp_ota_ops.h>
#include <driver/dac.h>
#include <driver/rtc_io.h>
#include <esp32/rom/rtc.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include <esp_heap_caps.h>
#include <esp32/clk.h>

#include "app_main.h"
#include "app_lightcontrol.h"
#include "app_utils.h"
#include "app_flash.h"
#include "app_wifi.h"
#include "app_utils.h"

uint32_t boot_count = 0;
uint32_t ota_update = 0;

esp_app_desc_t       app_info     = {};
esp_partition_t     *configured   = {};
esp_partition_t     *running      = {};
esp_ota_img_states_t ota_state    = {};


static void _print_divider (void)
{
  F_LOGI(true, true, LC_GREY, "---------------------------------------------");
}

/*an ota data write buffer ready to write to the flash*/
const char *ota_state_to_str (esp_ota_img_states_t ota_state)
{
  char *ptr = NULL;

  switch ( ota_state )
  {
    case ESP_OTA_IMG_NEW:
      ptr = (char *)"New";
      break;
    case ESP_OTA_IMG_PENDING_VERIFY:
      ptr = (char *)"Pending Verify";
      break;
    case ESP_OTA_IMG_VALID:
      ptr = (char *)"Valid";
      break;
    case ESP_OTA_IMG_INVALID:
      ptr = (char *)"Invalid";
      break;
    case ESP_OTA_IMG_ABORTED:
      ptr = (char *)"Aborted";
      break;
    case ESP_OTA_IMG_UNDEFINED:
      ptr = (char *)"Undefined";
      break;
  }

  return ptr;
}

void print_app_info (void)
{
  uint8_t default_mac_addr[6];
  esp_chip_info_t chip_info;

  // -----------------------------------------------------------
  // Get information about our enviroment
  // -----------------------------------------------------------
  configured = (esp_partition_t *)esp_ota_get_boot_partition ();
  running = (esp_partition_t *)esp_ota_get_running_partition ();

  esp_ota_get_state_partition (running, &ota_state);
  esp_ota_get_partition_description (running, &app_info);
  esp_chip_info (&chip_info);
  esp_efuse_mac_get_default (default_mac_addr);

  _print_divider ();
#if defined (CONFIG_DEBUG)
  F_LOGI(true, true, LC_GREY, "%25s = true", "Local build");
#endif
  F_LOGI(true, true, LC_GREY, "%25s = %d (last ota @ %d)", "Boot count", boot_count, ota_update);
  F_LOGI(true, true, LC_GREY, "%25s = CPU0: %d, CPU1: %d", "Reset reasons", (uint16_t)rtc_get_reset_reason (0), (uint16_t)rtc_get_reset_reason (1));
  F_LOGI(true, true, LC_GREY, "%25s = %s", "Firmware", app_info.version);
  F_LOGI(true, true, LC_GREY, "%25s = %s", "Project name", app_info.project_name);
  F_LOGI(true, true, LC_GREY, "%25s = %s", "IDF version", app_info.idf_ver);
  // ESP_LOGI("%25s = %s", "SHA256", app_info.app_elf_sha256);
  F_LOGI(true, true, LC_GREY, "%25s = %s %s", "Last full build", app_info.time, app_info.date);
  F_LOGI(true, true, LC_GREY, "%25s = %s %s", "Last part build", __TIME__, __DATE__);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Silicon rev", chip_info.revision);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Num cores", chip_info.cores);
  F_LOGI(true, true, LC_GREY, "%25s = WiFi%s%s", "Features", (chip_info.features & CHIP_FEATURE_BT)?"/BT":"", (chip_info.features & CHIP_FEATURE_BLE)?"/BLE":"");
  F_LOGI(true, true, LC_GREY, "%25s = %02X:%02X:%02X:%02X:%02X:%02X", "MAC address", default_mac_addr[0], default_mac_addr[1], default_mac_addr[2], default_mac_addr[3], default_mac_addr[4], default_mac_addr[5]);
  F_LOGI(true, true, LC_GREY, "%25s = %d MHz", "CPU freq", esp_clk_cpu_freq () / 1000000);
  F_LOGI(true, true, LC_GREY, "%25s = %dMB %s", "Flash size", spi_flash_get_chip_size () / (1024 * 1024), (chip_info.features & CHIP_FEATURE_EMB_FLASH)?"embedded":"external");
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Free heap size", esp_get_minimum_free_heap_size ());
  F_LOGI(true, true, LC_GREY, "%25s = %s (0x%08x)", "Partition", running->label, running->address);
  F_LOGI(true, true, LC_GREY, "%25s = %s", "OTA state", ota_state_to_str (ota_state));
  F_LOGI(true, true, LC_GREY, "%25s = %d", "LED count", control_vars.pixel_count);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "LED GPIO Pin", control_vars.led_gpio_pin);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Light GPIO Pin", control_vars.light_gpio_pin);
#if defined (CONFIG_USE_TASK_WDT)
  F_LOGI(true, true, LC_GREY, "%25s = %s", "CONFIG_USE_TASK_WDT", (CONFIG_USE_TASK_WDT?"yes":"no"));
#endif

  if ( configured != running )
  {
    F_LOGI(true, true, LC_YELLOW, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
    F_LOGI(true, true, LC_YELLOW, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
 // _print_divider ();
}

#if defined (CONFIG_DEBUG)
void show_nvs_usage (void)
{
  _print_divider ();
  F_LOGI(true, true, LC_GREY, "%16s: %-20s  %s", "Namespace", "Key", "Type");
  _print_divider ();

  nvs_iterator_t it = nvs_entry_find (NVS_PARTITION, NULL, NVS_TYPE_ANY);
  nvs_entry_info_t info;
  while ( it != NULL )
  {
    nvs_entry_info (it, &info);
    it = nvs_entry_next (it);
    F_LOGI(true, true, LC_GREY, "%16s: %-20s  %d", info.namespace_name, info.key, info.type);
  };

  _print_divider ();
  nvs_stats_t nvs_stats;
  nvs_get_stats (NULL, &nvs_stats);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Total Entries", nvs_stats.total_entries);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Used Entries", nvs_stats.used_entries);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Free Entries", nvs_stats.free_entries);
  F_LOGI(true, true, LC_GREY, "%25s = %d", "Namespace Count", nvs_stats.namespace_count);
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
// ESP32 Memory Map
// +-----------------+-------------+-------------+--------+--------------------+
// | Target          |Start Address| End Address | Size   |Target              |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 0 | 0x4007_0000 | 0x4009_FFFF | 192 kB | Instruction, Cache |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 1 | 0x3FFE_0000 | 0x3FFF_FFFF | 128 kB | Data, DAM          |
// |                 | 0x400A_0000 | 0x400B_FFFF |        | Instruction        |
// +-----------------+-------------+-------------+--------+--------------------+
// | Internal SRAM 2 | 0x3FFA_E000 | 0x3FFD_FFFF | 200 kB | Data, DMA          |
// +-----------------+-------------+-------------+--------+--------------------+
// see also C:\Espressif\esp-idf\components\soc\esp32\soc_memory_layout.c

void printHeapInfo (void)
{
  size_t free8min, free32min, free8, free32;

  _print_divider ();

  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
  // heap_caps_print_heap_info(MALLOC_CAP_32BIT);

  free8 = heap_caps_get_largest_free_block (MALLOC_CAP_8BIT);
  free32 = heap_caps_get_largest_free_block (MALLOC_CAP_32BIT);
  free8min = heap_caps_get_minimum_free_size (MALLOC_CAP_8BIT);
  free32min = heap_caps_get_minimum_free_size (MALLOC_CAP_32BIT);

  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Free heap", xPortGetFreeHeapSize ());
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Minimum 8-bit-capable", free8min);
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Largest 8-bit capable", free8);
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Minimum 32-bit-capable", free32min);
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Largest 32-bit capable", free32);
  F_LOGI(true, true, LC_GREY, "%25s = %d bytes", "Task stack", uxTaskGetStackHighWaterMark (NULL));
}
#endif // CONFIG_DEBUG

// **********************************************************************
// * Calculate all entries in a namespace.
// * An entry represents the smallest storage unit in NVS.Strings andblobs
// * may occupy more than one entry.Note that to find out the total number
// * of entries occupied by the namespace,
// **********************************************************************
void _show_namespace_used_entries (const char *ns)
{
  nvs_handle_t nvs_handle;
  nvs_open(ns, NVS_READONLY, &nvs_handle);
  size_t used_entries = 0;
  size_t total_entries_namespace = 0;
  if ( nvs_get_used_entry_count (nvs_handle, &used_entries) == ESP_OK )
  {
    // the total number of entries occupied by the namespace
    total_entries_namespace = used_entries + 1;
  }
  nvs_close (nvs_handle);
  F_LOGI(true, true, LC_BRIGHT_GREEN, "namespace: %s, used_entries: %d, total_entries: %d", ns, (int)used_entries, (int)total_entries_namespace);
}

// **********************************************************************
esp_err_t delete_nvs_events(const char *ns)
{
  esp_err_t err = ESP_ERR_NOT_FOUND;
  uint16_t itc = 0;
  //void *eventList = NULL;

  F_LOGW(true, true, LC_YELLOW, "Request to delete all events for '%s'", ns);

  // Count the number of events (if any)
  nvs_iterator_t it = nvs_entry_find (NVS_PARTITION, ns, NVS_TYPE_BLOB);
  while ( it != NULL )
  {
    itc++;
    it = nvs_entry_next (it);
  }

  if ( itc > 0 )
  {
    F_LOGW(true, true, LC_YELLOW, "Deleting all (%d) entries for '%s'", itc, ns);

    uint32_t nvs_handle;

    err = nvs_open(ns, NVS_READWRITE, &nvs_handle);
    if ( err == ESP_OK )
    {
      err = nvs_erase_all(nvs_handle);
    }
    if ( err == ESP_OK )
    {
      err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
  }

  return err;
}

// **********************************************************************
void *get_nvs_events (const char *ns, uint16_t eventLen, uint16_t *items)
{
  (*items) = 0;
  uint16_t itc = 0;
  void *eventList = NULL;

  // Namespace info...
#if defined (CONFIG_DEBUG)
  _show_namespace_used_entries (ns);
#endif

  // Count the number of events (if any)
  nvs_iterator_t it = nvs_entry_find (NVS_PARTITION, ns, NVS_TYPE_BLOB);
  while ( it != NULL )
  {
    itc++;
    it = nvs_entry_next (it);
  }

  // If we have any items, alloc some ram and add them to an array
  if ( itc > 0 )
  {
    uint32_t nvs_handle;
    uint16_t ci = 0; // Counter

    esp_err_t err = nvs_open (ns, NVS_READONLY, &nvs_handle);
    if ( err == ESP_OK )
    {
      // Save the count of items
      (*items) = itc;

      // Allocate some ram
      eventList = pvPortMalloc (itc * eventLen);
      if ( eventList == NULL )
      {
        F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating 'eventList'");
      }

      // Iterate through the event list
      nvs_iterator_t it = nvs_entry_find (NVS_PARTITION, ns, NVS_TYPE_BLOB);
      while ( it != NULL )
      {
        nvs_entry_info_t info;

        nvs_entry_info (it, &info);
        if ( info.type == NVS_TYPE_BLOB )
        {
          size_t blob_len = eventLen;
          err = nvs_get_blob (nvs_handle, info.key, ((uint8_t*)eventList) + (ci * eventLen), &blob_len);
          //F_LOGI(true, true, LC_GREY, "%16s: %-20s  %d (size: %d bytes)", info.namespace_name, info.key, info.type, blob_len);
          //hexDump (info.key, eventList + (ci * eventLen), blob_len, 16);
        }

        ci++;

        it = nvs_entry_next (it);
      }
      nvs_close (nvs_handle);
    }
  }

  return eventList;
}

void save_nvs_event (const char *ns, const char *eventKey, void *eventData, uint16_t dataLen)
{
  nvs_handle handle;
  esp_err_t err = nvs_open (ns, NVS_READWRITE, &handle);
  if ( err != ESP_OK )
  {
    F_LOGE(true, true, LC_YELLOW, "Couldn't open flash for \"%s\", key: \"%s\"", ns, eventKey);
  }
  else
  {
    err = nvs_set_blob(handle, eventKey, eventData, dataLen);
    if ( err != ESP_OK )
    {
      F_LOGE(true, true, LC_YELLOW, "Couldn't write flash in \"%s\", key: \"%s\"", ns, eventKey);
    }
    err = nvs_commit (handle);
    nvs_close (handle);
  }
}

esp_err_t save_nvs_str (const char *ns, const char *key, const char *strValue)
{
  nvs_handle handle;
  esp_err_t err = nvs_open (ns, NVS_READWRITE, &handle);
  if ( err == ESP_OK )
  {
    err = nvs_set_str(handle, key, strValue);
    err = nvs_commit(handle);
    nvs_close (handle);
  }
  return err;
}

esp_err_t get_nvs_num (const char *location, const char *token, char *value, tNumber type)
{
  nvs_handle handle;

  esp_err_t err = nvs_open (location, NVS_READONLY, &handle);
  if ( ESP_OK == err )
  {
    switch ( type )
    {
      case TYPE_U8:
        {
          uint8_t *x = (uint8_t *)value;
          err = nvs_get_u8 (handle, token, x);
          break;
        }
      case TYPE_U16:
        {
          uint16_t *x = (uint16_t *)value;
          err = nvs_get_u16 (handle, token, x);
          break;
        }
      case TYPE_NONE:
      default:
        err = ESP_FAIL;
        break;
    }

    nvs_close (handle);
  }
  return err;
}

esp_err_t save_nvs_num (const char *location, const char *token, const char *value, tNumber type)
{
  nvs_handle handle;

  F_LOGV(true, true, LC_BRIGHT_YELLOW, "save_nvs_num(%s, %s, %s, %d)", location, token, value, type);

  esp_err_t err = nvs_open (location, NVS_READWRITE, &handle);
  if ( ESP_OK == err )
  {
    switch ( type )
    {
      case TYPE_U8:
        {
          uint8_t x = (uint8_t)atoi (value);
          F_LOGV(true, true, LC_GREY, " U8: %s -> %d", token, x);
          err = nvs_set_u8(handle, token, x);
          break;
        }
      case TYPE_U16:
        {
          uint16_t x = (uint16_t)atoi (value);
          F_LOGV(true, true, LC_GREY, "U16: %s -> %d", token, x);
          err = nvs_set_u16(handle, token, x);
          break;
        }
      case TYPE_U32:
        {
          uint32_t x = (uint16_t)atoi (value);
          F_LOGV(true, true, LC_GREY, "U32: %s -> %d", token, x);
          err = nvs_set_u32(handle, token, x);
          break;
        }
      case TYPE_NONE:
      default:
        F_LOGE(true, true, LC_RED, "save_nvs_num(%s, %s, %s, %d) *Not Implemented*", location, token, value, type);
        err = ESP_FAIL;
        break;
    }

    if ( err == ESP_OK )
    {
      nvs_commit (handle);
    }

    nvs_close (handle);
  }
  return err;
}

void get_nvs_led_info (void)
{
  nvs_handle handle;
  uint16_t tmpInt;

  esp_err_t err = nvs_open (NVS_LIGHT_CONFIG, NVS_READONLY, &handle);
  if ( err == ESP_OK )
  {
    // The on schedule (On, auto, off)
    // -----------------------------------------------------------
    err = nvs_get_u8 (handle, "schedule", (uint8_t *)&tmpInt);
    if ( err == ESP_OK )
    {
      control_vars.schedule = tmpInt;

      // If our schedule is forced on or off, we need to enforce that
      if ( control_vars.schedule == 0 )
      {
        lightsUnpause(PAUSE_USER_REQ, true);
      }
      else if ( control_vars.schedule == 2 )
      {
        lightsPause(PAUSE_USER_REQ);
      }
    }

    // Count of LED 'pixels'
    // -----------------------------------------------------------
    err = nvs_get_u16 (handle, "led_count", &tmpInt);
    if ( err == ESP_OK )
    {
      control_vars.pixel_count = tmpInt;
      // FIXME: Need to calculate loop duration and how long
      // it takes to update all the LEDs
      if ( control_vars.pixel_count > 400 )
      {
        control_vars.pixel_count = 400;
      }
    }

    // LED brightness level
    // -----------------------------------------------------------
    err = nvs_get_u8 (handle, "dim_level", (uint8_t *)&tmpInt);
    if ( err == ESP_OK )
    {
      if ( check_valid_output(tmpInt) )
      {
        set_brightness(tmpInt);
      }
    }

    // LED lights GPIO pin
    // -----------------------------------------------------------
    err = nvs_get_u8 (handle, "led_gpio_pin", (uint8_t *)&tmpInt);
    if ( err == ESP_OK )
    {
      if ( check_valid_output(tmpInt) )
      {
        control_vars.led_gpio_pin = tmpInt;
      }
    }

    // Security light GPIO pin
    // -----------------------------------------------------------
    err = nvs_get_u8 (handle, "light_gpio_pin", (uint8_t *)&tmpInt);
    if ( err == ESP_OK )
    {
      if ( check_valid_output(tmpInt) )
      {
        control_vars.light_gpio_pin = tmpInt;
      }
    }

    // Light Zones
    // -----------------------------------------------------------
    char zoneStr[64];
    for ( uint8_t cz = 0; cz < control_vars.num_overlays; cz++ )
    {
      zone_params_t tmp_zone;
      size_t size = sizeof(zone_params_t);
      sprintf(zoneStr, "zone_%02d", cz);
      err = nvs_get_blob(handle, zoneStr, &tmp_zone, &size);
      if ( err == ESP_OK )
      {
        if ( size != sizeof(zone_params_t) )
        {
          F_LOGE(true, true, LC_YELLOW, "Invalid zone data for \"%s\"", zoneStr);
        }
        else
        {
          memcpy(&overlay[cz].zone_params, &tmp_zone, sizeof(tmp_zone));
        }
      }

      // Clear the other values
      overlay[cz].tHandle = NULL;
      overlay[cz].set = 0;
    }
    nvs_close (handle);
  }
}

void boot_init (void)
{
  // -----------------------------------------------------------
  // Initialise non volatile memory
  // -----------------------------------------------------------
  // err = nvs_flash_erase();

  esp_err_t err = nvs_flash_init();
  if ( err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND )
  {
    const esp_partition_t *nvs_partition = esp_partition_find_first (ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    ESP_ERROR_CHECK (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size));
    err = nvs_flash_init();
    F_LOGW(true, true, LC_RED, "NVS flash was erased, hope this was expected!")
  }
  else
  {
    F_LOGW(true, true, LC_BRIGHT_GREEN, "NVS flash was initialised okay.")
  }
  ESP_ERROR_CHECK(err);

  // -----------------------------------------------------------
  // Keep track of our reboots
  // -----------------------------------------------------------
  nvs_handle handle;
  if ( nvs_open(NVS_SYS_INFO, NVS_READWRITE, &handle) == ESP_OK )
  {
    nvs_get_u32(handle, NVS_OTA_UPDATE_KEY, &ota_update);
    nvs_get_u32(handle, NVS_BOOT_COUNT_KEY, &boot_count);
    boot_count++;
    nvs_set_u32(handle, NVS_BOOT_COUNT_KEY, boot_count);

    hostname_len = 32;
    if ( nvs_get_str(handle, NVS_HOSTNAME_KEY, hostname, &hostname_len) == ESP_ERR_NVS_NOT_FOUND )
    {
      F_LOGW(true, true, LC_YELLOW, "No hostname found in nvs");
      memcpy(hostname, CONFIG_TARGET_DEVICE_STR, strlen(CONFIG_TARGET_DEVICE_STR));
      nvs_set_str(handle, NVS_HOSTNAME_KEY, hostname);
    }
    else if ( hostname_len > 0 )
    {
      F_LOGW(true, true, LC_YELLOW, "Hostname found: %s", hostname);
    }

    nvs_commit(handle);
    nvs_close(handle);
  }

  // -----------------------------------------------------------
  // Enable OTA updates & other stuff
  // -----------------------------------------------------------
  spi_flash_init();
}

void new_fw_delay (void)
{
  // -----------------------------------------------------------
  // Did we just do an OTA update?
  // -----------------------------------------------------------
  if ( ota_state == ESP_OTA_IMG_PENDING_VERIFY )
  {
    // We'll wait for 30 seconds for any watchdogs to fail
    // before clearing the ESP_OTA_IMG_PENDING_VERIFY flag.
    // if we can run for 30 seconds we assume everything is
    // working fine.
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "New OTA firmware...");
    for ( uint8_t pw = 0; pw < 10; pw++ )
    {
      delay_ms (3000);
    }

    esp_ota_mark_app_valid_cancel_rollback ();
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "New firmware verified");
  }
}
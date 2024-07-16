


#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>

#include "app_main.h"
#include "app_sntp.h"
#include "app_mqtt.h"
#include "app_utils.h"

// ***************************************************************
// Random movement of PTZ CCTV device
// ***************************************************************
#if defined (CONFIG_CCTV)
// Call patrols
#define CALL_PATROL_1   35
#define CALL_PATROL_2   36
#define CALL_PATROL_3   37
#define CALL_PATROL_4   38
// These presets switch PTZ cameras between day/night mode
#define DAY_MODE        39
#define NIGHT_MODE      40
#define DAYNIGHT_AUTO   46
// Call patterns
#define CALL_PATTERN_1  41
#define CALL_PATTERN_2  42
#define CALL_PATTERN_3  43
#define CALL_PATTERN_4  44
#define CALL_PATTERN_5  102
#define CALL_PATTERN_6  103
#define CALL_PATTERN_7  104
#define CALL_PATTERN_8  105
// Remote reboot
#define REMOTE_REBOOT   94
// Default preset to return to
#define DEFAULT_PRESET  1
// Which hikmqtt camera to control
#define CCTV_DEVICE     4

// ***************************************************************
// CCD Settings
// ***************************************************************
// High-Light Compensation
#define HLC_LIGHT       39
#define HLC_DARK        65
// Day/night switch
#define DNS_DAY         0
#define DNS_NIGHT       1
#define DNS_AUTO        2
#define DNS_TIMING      3
#define DNS_TRIGGER     4

#define STR_HLC         "hlc"
#define STR_DAYNIGHT    "daynight"

#define RAND_ACTIVITY   false
#define CCTV_PTZ        false
#define SMOOTHING       false

// String to pass to hikmqtt to call preset <x>
#define strCallPreset   "{ \"command\": \"call_preset\" , \"devId\": %d, \"args\": { \"preset\": %d, \"channel\": 1 }}"
// String to pass to hikmqtt to start/stop channel recording
#define strStartRecord  "{ \"command\": \"start_record\" , \"devId\": %d, \"args\": { \"channel\": 1, \"rtype\": 0 }}"
#define strStopRecord   "{ \"command\": \"stop_record\" , \"devId\": %d, \"args\": { \"channel\": 1 }}"
// String to change CCD parameter
#define strSetParam     "{ \"command\": \"set_ccd_param\" , \"devId\": %d, \"args\": { \"param\": \"%s\", \"value\": \"%d\" }}"


IRAM_ATTR void cctv_dn_callback (void *arg)
{
  uint16_t devId = (uint16_t)(long int)arg;
  mqttPublish(dev_hikAlarm, strSetParam, devId, STR_DAYNIGHT, DNS_DAY);
}

// Make the CCTV camera act as a security light
void toggle_cctv_daynight(uint16_t devId)
{
  static esp_timer_handle_t handle = NULL;
  mqttPublish(dev_hikAlarm, strSetParam, devId, STR_DAYNIGHT, DNS_NIGHT);

#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
  const esp_timer_create_args_t setlight_timer_args = {
    .callback = &cctv_dn_callback,
    .arg = (void *)(long int)devId,
    .name = "cctvLight"};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour
  // Create one-shot timer
  // *****************************************
  if ( handle == NULL )
  {
    if ( esp_timer_create(&setlight_timer_args, &handle) != ESP_OK )
    {
      F_LOGE(true, true, LC_YELLOW, "Failed to create timer");
      return;
    }
  }
  else //if ( esp_timer_is_active(lHandle) )
  {
    esp_timer_stop(handle);
  }

  // One shot timer to turn the light off.
  esp_timer_start_once(handle, 45 * 1000000);
}

// The initial callback
void cctv_task(void *pvParameters)
{
  // Change HLC settings between day/night
  uint16_t hlc_control[] = {1,2,3,4};
  // Manually switch these devices between day/night mode
  uint16_t dns_control[] = {1,2,3};

  // Rnadom PTZ movement
  uint16_t dt_presets[]  = {2,3,4,5,6,7,8,10,11,26};
  uint16_t nt_presets[]  = {4,5,8,9,10,18,19,20,21};

  // Which presets to use and the number available
  uint16_t *presets = dt_presets;
  uint16_t count    = sizeof(dt_presets) / sizeof(uint16_t);

  // Processing CCD parameter changes
#if RAND_ACTIVITY == true
  uint64_t event_trigger = 0;
#endif

  // Store the value of our last light check
  uint16_t lastlight     = 0xff;    // Set to something impossible so as to trigger on first boot
  //uint16_t smoothing     = 0;

  // Set daytime mode in case it isn't set already
  mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, DAY_MODE);

  // Wait for a time sync
  while ( !isTimeSynced() )
  {
    delay_ms(1000);
  }

  // Delay again, for lightcheck.isdark to set if it is nighttime
  delay_ms(5000);

  // Loop forever(ish)
  for (;;)
  {
    // Get time since boot, in seconds
    uint64_t now = esp_timer_get_time() / 1000000;

    // Check if it became darker or lighter since last iteration.
    if ( lastlight != lightcheck.isdark )
    {
      // (60 is 1 minute, give or take)
      // Try and iron out momentary light level changes.
#if SMOOTHING == true
      if ( ++smoothing > 60 )
#endif // SMOOTHING
      {
        if ( lightcheck.isdark )
        {
          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Setting CCD %S Level to %d", STR_HLC, HLC_DARK);
          for ( int i = 0; i < sizeof(hlc_control) / sizeof(uint16_t); i++ )
          {
            mqttPublish(dev_hikAlarm, strSetParam, hlc_control[i], STR_HLC, HLC_DARK);
            delay_ms(200);
          }

          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Setting CCD %S Level to %d", STR_DAYNIGHT, DNS_NIGHT);
          for ( int i = 0; i < sizeof(dns_control) / sizeof(uint16_t); i++ )
          {
            mqttPublish(dev_hikAlarm, strSetParam, hlc_control[i], STR_DAYNIGHT, DNS_NIGHT);
            delay_ms(200);
          }
          presets = nt_presets;
          count = sizeof(nt_presets) / sizeof(uint16_t);
        }
        else
        {
          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Setting CCD %S Level to %d", STR_HLC, HLC_LIGHT);
          for ( int i = 0; i < sizeof(hlc_control) / sizeof(uint16_t); i++ )
          {
            mqttPublish(dev_hikAlarm, strSetParam, hlc_control[i], STR_HLC, HLC_LIGHT);
            delay_ms(200);
          }

          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Setting CCD %S Level to %d", STR_DAYNIGHT, DNS_DAY);
          for ( int i = 0; i < sizeof(dns_control) / sizeof(uint16_t); i++ )
          {
            mqttPublish(dev_hikAlarm, strSetParam, hlc_control[i], STR_DAYNIGHT, DNS_DAY);
            delay_ms(200);
          }
          presets = dt_presets;
          count = sizeof(dt_presets) / sizeof(uint16_t);
        }

        // Save current setting for next iteration
        lastlight = lightcheck.isdark;
      }
    }
#if SMOOTHING == true
    else
    {
      // Reset smoothing
      smoothing = 0;
    }
#endif // SMOOTHING

    // Do we need to trigger CCTV events?
#if RAND_ACTIVITY == true
    if ( event_trigger <= now )
    {
      // Only send commands if haven't just rebooted.
      if ( event_trigger > 0 )
      {
#if CCTV_PTZ == true
        F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Starting recording and movement");

        // Start manual recording
        mqttPublish(dev_hikAlarm, strStartRecord, CCTV_DEVICE);
        delay_ms(500);

        // Call a random preset
        mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, presets[(prng () % count)]);

        // Give enough time for the CCTV to move into position
        delay_ms(3000);

        // If night time, allow camera to move before switching to "night mode"
        if ( lightcheck.isdark )
        {
          // Switch to "night mode". This turns the CCTV IR light on
          mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, NIGHT_MODE);
          // Flicker...
          for ( int c = 0; c < 2; c++ )
          {
            delay_ms(500);
            mqttPublish( dev_hikAlarm, strCallPreset, CCTV_DEVICE, DAY_MODE);
            delay_ms(500);
            mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, NIGHT_MODE);
          }
        }

        // Now wait a random time beofre returning to our default preset
        delay_ms((prng () % 12000) + 2000);

        // Set day mode regardless
        mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, DAY_MODE);
        delay_ms(500);

        // Return to default preset
        mqttPublish(dev_hikAlarm, strCallPreset, CCTV_DEVICE, DEFAULT_PRESET);
        delay_ms(500);

        // Stop manual recording
        mqttPublish(dev_hikAlarm, strStopRecord, CCTV_DEVICE);
#else // CCTV_PTZ
        if ( lightcheck.isdark )
        {
          // Cause camera light to come on by switching device to night-time mode
          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Randomness. Switching device to night mode");
          mqttPublish(dev_hikAlarm, strSetParam, 4, STR_DAYNIGHT, DNS_NIGHT);

          // Now wait a random time beofre returning to our preferred setting
          delay_ms((prng() % 15000) + 5000);

          F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV: Randomness. Switching device to day mode");
          mqttPublish(dev_hikAlarm, strSetParam, 4, STR_DAYNIGHT, DNS_DAY);
        }
#endif // CCTV_PTZ
      }

      // Random wait period between runs
      uint32_t pause_for = (prng() % 7200) + (60 * 20);   // random 2 hour + 20 minutes
      uint16_t pause_min = pause_for / 60;
      uint16_t pause_sec = pause_for % 60;
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "CCTV Random Delay: %d minutes and %d seconds", pause_min, pause_sec);
      event_trigger = now + pause_for;
      //F_LOGW(true, true, LC_BRIGHT_YELLOW, "%lld --> %lld", now, event_trigger);
    }
#endif // RAND_ACTIVITY

    // Wait a second.
    delay_ms(1000);
  }
}
#endif /* CONFIG_CCTV */
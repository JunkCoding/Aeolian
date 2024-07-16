


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <driver/gpio.h>
#include <esp32/rom/rtc.h>
#include <nvs.h>
#include <nvs_flash.h>

#if defined (CONFIG_APPTRACE_SV_ENABLE)
#include <esp_sysview_trace.h>
#endif

#include "app_main.h"
#include "app_flash.h"
#include "app_lightcontrol.h"
#include "app_schedule.h"
#include "app_timer.h"
#include "app_sntp.h"
#include "app_mqtt.h"
#include "sundial.h"
#include "app_utils.h"

#define NHS_BEGIN         ((20  * 60) + 0)    // 8:00pm
#define NHS_END           ((20 * 60) + 30)    // 8:30pm

#define TEST_BEGIN        ((21 * 60) + 00)    // 9:00pm
#define TEST_END          ((21 * 60) + 01)    // 9:01pm

#define NVS_WEEKLY_DATA   "weekly_data"
#define NVS_ANNUAL_DATA   "annual_data"

const char *dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *monNames[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

// ***********************************************************
// *  Default weekly events                                  *
// ***********************************************************
weekly_event_t _initial_weekly_events[] = {
//     Start    |      End     |
//  Hour   Min  |  Hour   Min  |  Day  |  Theme          |  Brightness      |  Flags
  {{20,    00},   {20,    30},    1,      THEME_DEFAULT,    DIM_HIGH,          EVENT_LIGHTSON | EVENT_INACTIVE},
  {{20,    00},   {20,    30},    4,      THEME_DEFAULT,    DIM_HIGH,          EVENT_LIGHTSON | EVENT_INACTIVE},
};
#define NUM_WEEKLY_EVENTS (sizeof(_initial_weekly_events) / sizeof(weekly_event_t))

// ***********************************************************
// *  Default annual events                                  *
// ***********************************************************
annual_event_t _initial_annual_events[] = {
//     Start   |       End      | Start |  End  |
//  Hour   Min |   Hour   Min   |  Day  |  Day  | Month | Theme            |  Brightness  |  Flags
  {{07,    00},   {21,    30},     01,     31,    0,      THEME_UKRAINE,      DIM_MED,       EVENT_AUTONOMOUS | EVENT_DEFAULT},    // Default event
  {{00,    00},   {00,    00},     05,     06,    0,      THEME_CHRISTMAS,    DIM_MED,       EVENT_LIGHTSOFF},                     // Off for Twelfth Night
//{{07,    00},   {21,    30},     12,     12,    0,      THEME_COCO,         DIM_MED,       EVENT_AUTONOMOUS},                    // Coco commemorative
//{{07,    00},   {21,    30},     10,     10,    4,      THEME_JESS,         DIM_MED,       EVENT_AUTONOMOUS},                    // Jess commemorative
  {{07,    00},   {22,    00},     31,     31,    9,      THEME_HALLOWEEN,    DIM_MED,       EVENT_AUTONOMOUS},                    // All-Hallows Eve
  {{07,    00},   {22,    00},     01,     31,    11,     THEME_CHRISTMAS,    DIM_MED,       EVENT_AUTONOMOUS},                    // Christmas (normal Christmas schedule)
  {{07,    00},   {22,    30},     24,     26,    11,     THEME_CHRISTMAS,    DIM_MAX,       EVENT_LIGHTSON}                       // Christmas (on all day for Eve, Day & Box. Day)
};
#define NUM_ANNUAL_EVENTS (sizeof(_initial_annual_events) / sizeof(annual_event_t))

weekly_event_t *weekly_events   = NULL;                     // Pointer to list of weekly events
uint16_t num_weekly_events      = NUM_WEEKLY_EVENTS;        // Count of weekly events
uint16_t cur_weekly_event       = 0;                        // Current event we are comparing with today

annual_event_t *annual_events   = NULL;                     // Pointer to list of annual events
uint16_t num_annual_events      = NUM_ANNUAL_EVENTS;        // Count of annual events
uint16_t cur_annual_event       = 0;                        // Current event we are comparing our current date against
uint16_t def_annual_event       = 0;                        // In the absence of an active "cur_annual_event", use this setting

// ***********************************************************
// * Display companion utilities                             *
// ***********************************************************
static const char *nth ( uint8_t day )
{
  const char *nthStr[] = {"th", "st", "nd", "rd"};

  uint8_t cd = day % 10;
  switch ( cd )
  {
    case 0x01:
    case 0x02:
    case 0x03:
      return nthStr[cd];
    default:
      return nthStr[0];
  }
}
// ***********************************************************
// * Human readable flags for those that want to know        *
// ***********************************************************
static const char *flags2str ( char *destStr, schedulerFlags_t flags )
{
  uint8_t ptr = 0;

  if ( BTST (flags, EVENT_AUTONOMOUS) )
  {
    ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "automonous");
  }
  else
  {
    ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "manual");

    if ( BTST (flags, EVENT_LIGHTSOFF) )
    {
      ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "lights off");
    }
    else
    {
      ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "lights om");
    }
  }

  if ( BTST (flags, EVENT_DEFAULT) )
  {
    ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "default");
  }

  if ( BTST (flags, EVENT_IMMUTABLE) )
  {
    ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "immutable");
  }

  if ( BTST (flags, EVENT_INACTIVE) )
  {
    ptr += sprintf(&destStr[ptr], "%s%s", ptr?", ":"", "disabled");
  }

  destStr[ptr] = 0;
  return destStr;
}

// ***********************************************************
// * Sorting functions                                       *
// ***********************************************************
static int sWeekSortFunc ( const void *a, const void *b )
{
  const weekly_event_t *pA = (const weekly_event_t *)a;
  const weekly_event_t *pB = (const weekly_event_t *)b;
  if ( pA->day != pB->day )
  {
    return (int)pA->day - (int)pB->day;
  }
  else if ( pA->start.hour != pB->start.hour )
  {
    return (int)pA->start.hour - (int)pB->start.hour;
  }
  else
  {
    return (int)pA->start.minute - (int)pB->start.minute;
  }
}

static int sMonthSortFunc ( const void *a, const void *b )
{
  const annual_event_t *pA = (const annual_event_t *)a;
  const annual_event_t *pB = (const annual_event_t *)b;
  if ( pA->month != pB->month )
  {
    return (int)pA->month - (int)pB->month;
  }
  else if ( pA->dayStart != pB->dayStart )
  {
    return (int)pA->dayStart - (int)pB->dayStart;
  }
  else
  {
    return (int)pA->start.hour - (int)pB->start.hour;
  }
}

// ***********************************************************
// *  Zone lights (typically a white light overlay)          *
// ***********************************************************
bool zoneStatus ( uint8_t id )
{
  bool on = false;

  if ( overlay[id].set > 0 )
  {
    on = true;
  }
  else
  {
    on = overlay[id].state;
  }

  return on;
}

// ***********************************************************
// ***********************************************************
IRAM_ATTR void setzone_callback ( void *arg )
{
  uint8_t id = (uint8_t)(long int)arg;
  overlay[id].set = 0;
}

#if defined (CONFIG_APPTRACE_SV_ENABLE)
void setZone ( uint8_t id, uint8_t mask, cRGB color, uint16_t duration )
#else
void setZone ( uint8_t id, uint8_t mask, cRGB color, uint16_t duration )
#endif
{
  //return;
  if ( overlay[id].tHandle == NULL )
  {
    // Configure one-shot timer
    // *****************************************
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
    const esp_timer_create_args_t setzone_timer_args = {
      .callback = &setzone_callback,
      .arg = (void *)(long int)id,
      .name = "zoneset"};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

    // Create one-shot timer
    // *****************************************
    if ( esp_timer_create ( &setzone_timer_args, &overlay[id].tHandle ) != ESP_OK )
    {
      F_LOGE(true, true, LC_YELLOW, "Failed to create timer" );
      return;
    }
  }
  else //if ( esp_timer_is_active(overlay[id].handle) )
  {
    esp_timer_stop ( overlay[id].tHandle );
  }

  // Configure overlay...
  // *****************************************
  overlay[id].zone_params.mask = mask;
  overlay[id].zone_params.color = color;
  overlay[id].set = 1;

  // Convert duration from milli to micro...
  // *****************************************
  esp_timer_start_once ( overlay[id].tHandle, duration * 1000 );
}

// ***********************************************************
// *  Security lights (ie. flood lights)                     *
// ***********************************************************
bool lightStatus ( uint8_t id )
{
  return (flood[id].isOn == true);
}

void light_off ( uint8_t id, const char *message )
{
  flood[id].isOn = false;
  flood[id].eTime = mp_hal_ticks_ms ();
  gpio_set_level ( (gpio_num_t)control_vars.light_gpio_pin, 0 );
  mqttPublish ( dev_Light, message );
}

IRAM_ATTR void setlight_callback ( void *arg )
{
  uint8_t id = (uint8_t)(long int)arg;
  light_off(id, "Off (timeout)");
}

#if defined (CONFIG_APPTRACE_SV_ENABLE)
void set_light(uint8_t id, uint8_t mode, uint8_t time)
#else
void set_light(uint8_t id, uint8_t mode, uint8_t time)
#endif
{
  uint64_t now = mp_hal_ticks_ms();

  if ( mode && time )
  {
    // 30 second settling time to counter CCTV triggered by lights turned off
    if ( now > (flood[id].eTime + 30000) )
    {
      mqttPublish ( dev_Light, "on" );

      if ( flood[id].handle == NULL )
      {
        // Configure one-shot timer
        // *****************************************
#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
        const esp_timer_create_args_t setlight_timer_args = {
          .callback = &setlight_callback,
          .arg = (void *)(long int)id,
          .name = "lightset"};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

        // Create one-shot timer
        // *****************************************
        if ( esp_timer_create(&setlight_timer_args, &flood[id].handle) != ESP_OK )
        {
          F_LOGE(true, true, LC_YELLOW, "Failed to create timer");
          return;
        }
      }
      else //if ( esp_timer_is_active(lHandle) )
      {
        esp_timer_stop(flood[id].handle);
      }

      gpio_set_level((gpio_num_t)control_vars.light_gpio_pin, 1);  // FIXME: include in the struct

      flood[id].isOn = true;
      flood[id].sTime = now;
      flood[id].eTime = now + (time * 1000);

      // Convert duration from seconds to micro...
      // *****************************************
      esp_timer_start_once(flood[id].handle, time * 1000000);
    }
  }
  else if ( flood[id].isOn )
  {
    esp_timer_stop(flood[id].handle);
    light_off(id, "Off (request)");
  }
}

// ***********************************************************
// * Initialse and configure the scheduler                   *
// ***********************************************************
esp_err_t init_scheduler ( void )
{
  esp_err_t err = ESP_OK;
  char tmpStr[255];
  uint16_t items;
  uint16_t it;

#if defined CONFIG_APPTRACE_SV_ENABLE
  esp_sysview_heap_trace_start(-1);
#endif

  // Enable if you want to restore/set defaults
  //delete_nvs_events(NVS_ANNUAL_SCHEDULE);
  //delete_nvs_events(NVS_WEEKLY_SCHEDULE);

  // Check if we have any weekly events saved in NVS
  weekly_event_t *tmpWeekly = (weekly_event_t *)get_nvs_events ( NVS_WEEKLY_SCHEDULE, sizeof ( weekly_event_t ), &items );
  if ( tmpWeekly == NULL )
  {
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "Initialising weekly events..." );

    weekly_events = _initial_weekly_events;
    for ( it = 0; it < num_weekly_events; it++ )
    {
      sprintf( tmpStr, "%02d%02d%02d", weekly_events[it].start.hour, weekly_events[it].start.minute, weekly_events[it].day );
      save_nvs_event ( NVS_WEEKLY_SCHEDULE, tmpStr, (void *)&weekly_events[it], sizeof ( weekly_event_t ) );
    }
  }
  else
  {
    num_weekly_events = items;
    weekly_events = tmpWeekly;
  }

  // Sort our weekly events into sequential order
  qsort ( weekly_events, num_weekly_events, sizeof ( weekly_event_t ), sWeekSortFunc );
  F_LOGI(true, true, LC_MAGENTA, "Weekly events");
  for ( uint8_t i = 0; i < num_weekly_events; i++ )
  {
    // FIXME: Sanitize events
    // valid theme, week, month, etc.
    F_LOGI(true, true, LC_CYAN, "%12s %02d:%02d -> %02d:%02d  Theme: %s (%s)",
      dayNames[weekly_events[i].day],
      weekly_events[i].start.hour, weekly_events[i].start.minute,
      weekly_events[i].end.hour, weekly_events[i].end.minute,
      get_theme_name ( weekly_events[i].theme ), flags2str(tmpStr, (schedulerFlags_t)weekly_events[i].flags));
  }

  // Check if we have any annual events saved in NVS
  annual_event_t *tmpAnnual = (annual_event_t *)get_nvs_events ( NVS_ANNUAL_SCHEDULE, sizeof ( annual_event_t ), &items );
  if ( tmpAnnual == NULL )
  {
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "Initialising annual events..." );

    annual_events = _initial_annual_events;
    for ( it = 0; it < num_annual_events; it++ )
    {
      sprintf( tmpStr, "%02d%02d%02d%02d", annual_events[it].start.hour, annual_events[it].start.minute, annual_events[it].dayStart, annual_events[it].month );
      save_nvs_event ( NVS_ANNUAL_SCHEDULE, tmpStr, (void *)&annual_events[it], sizeof ( annual_event_t ) );
    }
  }
  else
  {
    num_annual_events = items;
    annual_events = tmpAnnual;
  }

  // Sort our annual events into sequential order
  qsort ( annual_events, num_annual_events, sizeof ( annual_event_t ), sMonthSortFunc );
  F_LOGI(true, true, LC_MAGENTA, "Annual events");
  for ( uint8_t i = 0; i < num_annual_events; i++ )
  {
    F_LOGI(true, true, LC_CYAN, "%12s %2d%2s -> %2d%2s, %02d:%02d -> %02d:%02d  Theme: %s (%s)",
      monNames[annual_events[i].month],
      annual_events[i].dayStart, nth ( annual_events[i].dayStart ),
      annual_events[i].dayEnd, nth ( annual_events[i].dayEnd ),
      annual_events[i].start.hour, annual_events[i].start.minute,
      annual_events[i].end.hour, annual_events[i].end.minute,
      get_theme_name ( annual_events[i].theme ), flags2str(tmpStr, (schedulerFlags_t)annual_events[i].flags));
  }

  return err;
}

// ***********************************************************
// * This controls weekly theme changes (run once a minute)  *
// ***********************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
static bool check_weekly_event (struct tm tm)
#else
static bool check_weekly_event (struct tm tm)
#endif
{
  static bool last_match = false;
  char tmpStr[255];
  uint16_t eval_pos;
  uint16_t valid_pos = 0;
  bool match = false;

  // Only process if we are not a slave controller
  if ( !BTST(control_vars.bitflags, DISP_BF_SLAVE) )
  {
    uint16_t tod = (tm.tm_hour * 60) + tm.tm_min;

    // Reset index at the beginning of the week
    if ( cur_weekly_event < num_weekly_events )
    {
      eval_pos = cur_weekly_event;
    }
    else
    {
      eval_pos = 0;
    }

    // This is too verbose for normal logging.
    F_LOGV(true, true, LC_CYAN, "Checking for 'weekly event' theme change");

    // Iterate through weekly events, if necessary
    for ( ;eval_pos < num_weekly_events; eval_pos++ )
    {
#if defined (CONFIG_DEBUG)
    F_LOGV(true, true, LC_GREY, "Evaluating: %s, %02d:%02d to %02d:%02d, theme: %s (%s)",
      dayNames[weekly_events[eval_pos].day],
      weekly_events[eval_pos].start.hour, weekly_events[eval_pos].start.minute,
      weekly_events[eval_pos].end.hour, weekly_events[eval_pos].end.minute,
      get_theme_name(weekly_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)weekly_events[eval_pos].flags));
#endif
      // Is pending event for today?
      if ( weekly_events[eval_pos].day == tm.tm_wday && !BTST(weekly_events[eval_pos].flags, EVENT_INACTIVE) )
      {
        uint16_t sst = (weekly_events[eval_pos].start.hour * 60) + weekly_events[eval_pos].start.minute;
        uint16_t est = (weekly_events[eval_pos].end.hour * 60) + weekly_events[eval_pos].end.minute;

#if defined (CONFIG_DEBUG)
        F_LOGD(true, true, LC_BRIGHT_YELLOW, "tod = %d, sst = %d, est = %d (cur_weekly_event: %d, num_weekly_events: %d)", tod, sst, est, cur_weekly_event, num_weekly_events);
#endif
        // Current time less than end time?
        if ( tod < est )
        {
          // Current time greater than start time?
          if ( tod >= sst )
          {
            // Flag a match
            match = true;

            // Save this as the last known valid match
            valid_pos = eval_pos;

#if defined (CONFIG_DEBUG)
            F_LOGW(true, true, LC_GREEN, "Match: %s, %02d:%02d to %02d:%02d, theme: %s (%s)",
              dayNames[weekly_events[eval_pos].day],
              weekly_events[eval_pos].start.hour, weekly_events[eval_pos].start.minute,
              weekly_events[eval_pos].end.hour, weekly_events[eval_pos].end.minute,
              get_theme_name(weekly_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)weekly_events[eval_pos].flags));
#endif
          }
          else
          {
            // Same day, but the start time has not yet arrived
            break;
          }
        }
      }
      // Break out of the loop if the event being evaluated is for a later day of the week
      else if ( weekly_events[eval_pos].day > tm.tm_wday )
      {
        break;
      }
    }

    if ( match == true )
    {
      cur_weekly_event = valid_pos;
      set_theme(weekly_events[cur_weekly_event].theme, weekly_events[cur_weekly_event].dim);
    }
    else if ( eval_pos == num_weekly_events )
    {
      cur_weekly_event = 0;
    }
    else
    {
      cur_weekly_event = eval_pos;
    }

#if defined (CONFIG_DEBUG)
    F_LOGV(true, true, LC_WHITE, "Next event: %s, %02d:%02d to %02d:%02d, theme: %s (%s)",
      dayNames[weekly_events[eval_pos].day],
      weekly_events[eval_pos].start.hour, weekly_events[eval_pos].start.minute,
      weekly_events[eval_pos].end.hour, weekly_events[eval_pos].end.minute,
      get_theme_name(weekly_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)weekly_events[eval_pos].flags));
#endif

    if ( match == true )
    {
      lightsUnpause(PAUSE_SCHEDULE, false);
    }
    else if ( last_match == true )
    {
      if ( lightsPausedReason(0xFF) )
      {
        lightsPause(PAUSE_SCHEDULE);
      }
    }

    // Save current match status
    last_match = match;
  }

  return match;
}

// ***********************************************************
// * This controls annual theme changes (run once a day)    *
// ***********************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
static bool check_annual_event(struct tm tm, bool ignore_master=false)
#else
static bool check_annual_event(struct tm tm, bool ignore_master=false)
#endif
{
  char tmpStr[255];
  uint16_t eval_pos;
  uint16_t valid_pos = 0;
  bool match = false;

  // Only process if we are not a slave controller
  if ( !BTST(control_vars.bitflags, DISP_BF_SLAVE) || ignore_master )
  {
    // Reset index at the beginning of the year
    if ( cur_annual_event < num_annual_events )
    {
      eval_pos = cur_annual_event;
    }
    else
    {
      eval_pos = 0;
    }

    F_LOGI(true, true, LC_CYAN, "Checking for 'annual event' theme change");

    // Iterate through events, if necessary
    for ( ;eval_pos < num_annual_events; eval_pos++ )
    {
#if defined (CONFIG_DEBUG)
      F_LOGV(true, true, LC_GREY, "Evaluating: %s %2d%2s to %2d%2s, %02d:%02d to %02d:%02d, theme: %s (%s)",
          monNames[annual_events[eval_pos].month],
          annual_events[eval_pos].dayStart, nth (annual_events[eval_pos].dayStart),
          annual_events[eval_pos].dayEnd, nth (annual_events[eval_pos].dayEnd),
          annual_events[eval_pos].start.hour, annual_events[eval_pos].start.minute,
          annual_events[eval_pos].end.hour, annual_events[eval_pos].end.minute,
          get_theme_name (annual_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)annual_events[eval_pos].flags));
#endif
      // Does this event match the current month and enabled?
      if ( (annual_events[eval_pos].month == tm.tm_mon) && !BTST(annual_events[eval_pos].flags, EVENT_INACTIVE) )
      {
        // Current day less than end day time?
        if ( tm.tm_mday <= annual_events[eval_pos].dayEnd )
        {
          // Current day greater than start day?
          if ( tm.tm_mday >= annual_events[eval_pos].dayStart )
          {
            // Flag a match
            match = true;

            // Save this as the last known valid match
            valid_pos = eval_pos;
#if defined (CONFIG_DEBUG)
            F_LOGW(true, true, LC_GREEN, "Match: %s %2d%2s to %2d%2s, %02d:%02d to %02d:%02d, theme: %s (%s)",
                monNames[annual_events[eval_pos].month],
                annual_events[eval_pos].dayStart, nth (annual_events[eval_pos].dayStart),
                annual_events[eval_pos].dayEnd, nth (annual_events[eval_pos].dayEnd),
                annual_events[eval_pos].start.hour, annual_events[eval_pos].start.minute,
                annual_events[eval_pos].end.hour, annual_events[eval_pos].end.minute,
                get_theme_name (annual_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)annual_events[eval_pos].flags));
#endif
            // Should this be set as the default event?
            if ( BTST (annual_events[eval_pos].flags, EVENT_DEFAULT) )
            {
              def_annual_event = eval_pos;
            }
          }
          else
          {
            break;
          }
        }
      }
      // Break out if the event being evaluated is for a later month than the current one.
      else if ( annual_events[eval_pos].month > tm.tm_mon )
      {
        break;
      }
    }

    if ( eval_pos == num_annual_events )
    {
      eval_pos = 0;
    }

    if ( match == true )
    {
      cur_annual_event = valid_pos;
    }

    // Log our current comparison
#if defined (CONFIG_DEBUG)
    F_LOGV(true, true, LC_WHITE, "Next event: %s %2d%2s to %2d%2s, %02d:%02d to %02d:%02d, theme: %s (%s)",
      monNames[annual_events[eval_pos].month],
      annual_events[eval_pos].dayStart, nth (annual_events[eval_pos].dayStart),
      annual_events[eval_pos].dayEnd, nth (annual_events[eval_pos].dayEnd),
      annual_events[eval_pos].start.hour, annual_events[eval_pos].start.minute,
      annual_events[eval_pos].end.hour, annual_events[eval_pos].end.minute,
      get_theme_name (annual_events[eval_pos].theme), flags2str(tmpStr, (schedulerFlags_t)annual_events[eval_pos].flags));
#endif
  }

  F_LOGV(true, true, LC_BRIGHT_GREEN, "match = %d", match);

  return match;
}

void process_annual(struct tm tmz, uint16_t ae)
{
  // Minutes past midnight... (or Time of Day)
  // ---------------------------------------------------------------------------
  uint32_t tod = (tmz.tm_hour * 60) + tmz.tm_min;

  // Convert the start and end time to something more useable
  // ---------------------------------------------------------------------------
  uint16_t sst = (annual_events[ae].start.hour * 60) + annual_events[ae].start.minute;
  uint16_t est = (annual_events[ae].end.hour * 60) + annual_events[ae].end.minute;
  F_LOGV(true, true, LC_BRIGHT_GREEN, "tod = %d, sst = %d, est = %d", tod, sst, est);

  // Event inactive: We didn't set any events to be on
  // ---------------------------------------------------------------------------
  if ( BTST(annual_events[ae].flags, EVENT_INACTIVE) )
  {
    F_LOGE(true, true, LC_YELLOW, "No active annual events!");
    return;
  }

  // Log our active annual event
  // ---------------------------------------------------------------------------
  F_LOGV(true, true, LC_BRIGHT_GREEN, "Active annual event: %s %d%2s to %d%2s, %02d:%02d to %02d:%02d, theme: %s",
    monNames[annual_events[ae].month],
    annual_events[ae].dayStart, nth (annual_events[ae].dayStart),
    annual_events[ae].dayEnd, nth (annual_events[ae].dayEnd),
    annual_events[ae].start.hour, annual_events[ae].start.minute,
    annual_events[ae].end.hour, annual_events[ae].end.minute,
    get_theme_name (annual_events[ae].theme));

  // Calculate if we fall inbetween the valid start and end time for
  // the current event.
  // ---------------------------------------------------------------------------
  bool validTimeWindow = false;
  if ( (!est || tod < est) && (tod >= sst) )
  {
    validTimeWindow = true;
  }
  F_LOGD(true, true, LC_WHITE, "%02d:%02d, within time boundaries = %s", tmz.tm_hour, tmz.tm_min, validTimeWindow?"true":"false");

  // Ensure we displaying the correct theme.
  // ---------------------------------------------------------------------------
  if ( control_vars.cur_themeID != annual_events[ae].theme )
  {
    set_theme(annual_events[ae].theme, annual_events[ae].dim);
  }

  // Check our forced schedule settings (which override all other settings)
  // ---------------------------------------------------------------------------
  if ( control_vars.schedule == SCHED_OFF && !BTST(control_vars.bitflags, DISP_BF_PAUSED) )
  {
    F_LOGI(true, true, LC_WHITE, "control_vars.schedule = SCHED_OFF");
    lightsPause(PAUSE_SCHEDULE);
  }
  else if ( control_vars.schedule == SCHED_ON && BTST(control_vars.bitflags, DISP_BF_PAUSED) )
  {
    F_LOGI(true, true, LC_WHITE, "control_vars.schedule = SCHED_ON");
    lightsUnpause(PAUSE_SCHEDULE, false);
  }
  // Else, if we are inside a valid time window
  // ---------------------------------------------------------------------------
  else if ( validTimeWindow )
  {
    // Autonomous mode, we turn the lights on between sunset and sunrise.
    // ---------------------------------------------------------------------------
    if ( BTST(annual_events[ae].flags, EVENT_AUTONOMOUS) )
    {
      F_LOGV(true, true, LC_WHITE, "sunrise/sunset: auto");

      // Is it dark?
      if ( lightcheck.isdark )
      {
        // It's dark and the lights off!!
        if ( BTST(control_vars.bitflags, DISP_BF_PAUSED) )
        {
          // Turn the lights on
          lightsUnpause(PAUSE_DAYTIME, false);
        }
      }
      // Turn the lights off
      else if ( !BTST(control_vars.bitflags, DISP_BF_PAUSED) )
      {
        lightsPause(PAUSE_DAYTIME);
      }
    }
    // Lights will be forced off during this period
    // ---------------------------------------------------------------------------
    else if ( BTST(annual_events[ae].flags, EVENT_LIGHTSOFF) )
    {
      F_LOGV(true, true, LC_WHITE, "fixed hours: off");
      lightsPause(PAUSE_EVENT);
    }
    // Lights will be forced on during this period
    // ---------------------------------------------------------------------------
    else  // if ( BTST (annual_events[ae].flags, EVENT_LIGHTSON) )
    {
      F_LOGV(true, true, LC_WHITE, "fixed hours: on");
      lightsUnpause(PAUSE_SCHEDULE, false);
    }
  }
  // If our current time is not between a valid on period, turn the lights off.
  // ---------------------------------------------------------------------------
  else if ( !BTST(control_vars.bitflags, DISP_BF_PAUSED) )
  {
    F_LOGI(true, true, LC_WHITE, "Lights off");

    lightsPause(PAUSE_DAYTIME);
  }
}

// ***********************************************************
// ***********************************************************
//void scheduler_task ( void *pvParameters )
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void scheduler_task(void *arg)
#else
void scheduler_task(void *arg)
#endif
{
#if not defined (CONFIG_AXRGB_DEV_CARAVAN)
  // Toggle onboard LED to show we are alive
  // *****************************************
  static bool status_led = false;
  gpio_set_level((gpio_num_t)2, status_led);
  status_led = !status_led;
#endif
#if defined (CONFIG_APPTRACE_SV_ENABLE)
  esp_sysview_flush(ESP_APPTRACE_TMO_INFINITE);
#endif
  // Used for smoothing out spikes in the light level
  // *****************************************
  static uint16_t smoothing = 0xff;

  // Set minute_check to 1 so it is processed first time
  // through the function call
  // *****************************************
  static uint8_t minute_check = 1;

  // Set once a day and is short for Current Working Month
  // *****************************************
  static uint16_t ae = 0;

  // Reboot?
  // *****************************************
  if ( BTST(control_vars.bitflags, DISP_BF_REBOOT) )
  {
    // Log what we are doing
    F_LOGV(true, true, LC_YELLOW, "Reboot..." );

    // Turn the lights off before rebooting
    // FIXME: This doesn't work as it should.
    lightsPause(PAUSE_REBOOT);

    // Reboot
    esp_restart();
  }
  // Wait for NTP time sync before doing any checking
  // *****************************************
  else if ( isTimeSynced() )
  {
    time_t now;
    time(&now);
    time_t tod = now % 86400;
    struct tm tmz = *localtime(&now);

    F_LOGV(true, true, LC_CYAN, "now: %ld, nextcheck: %ld", now, lightcheck.nextcheck );

    // Only process this code if we are a slave
    if ( MQTT_Client_Cfg.Slave )
    {
      // We always need to decrement the counter, so do that first and only if greater than zero
      if ( control_vars.master_alive )
      {
        control_vars.master_alive--;
      }

      // Did our master timeout and we are flagged as slave?
      if ( control_vars.master_alive && !BTST(control_vars.bitflags, DISP_BF_SLAVE) )
      {
        F_LOGW(true, true, LC_GREEN, "Slave status: Master available, accepting and processing received commands");
        BSET(control_vars.bitflags, DISP_BF_SLAVE);
      }
      else if ( !control_vars.master_alive && BTST(control_vars.bitflags, DISP_BF_SLAVE) )
      {
        F_LOGW(true, true, LC_YELLOW, "Slave status: Master timed out, switching to autonomous mode");
        BCLR(control_vars.bitflags, DISP_BF_SLAVE);
      }
    }

    // Periodic checking of sunrise/sunset
    // (we should only need to do this once a day and will always run at first boot)
    // *****************************************
    if ( now >= lightcheck.nextcheck )
    {
      // -----------------------------------------------------------
      // If we just (re)booted this will not be set
      // -----------------------------------------------------------
      if ( !BTST(control_vars.bitflags, DISP_BF_INITIALISED) )
      {
        F_LOGW(true, true, LC_BRIGHT_GREEN, "Initialisation complete: Enjoy the show!");

        if ( MQTT_Client_Cfg.Master == true )
        {
          control_vars.slaves.paused      = BTST(control_vars.bitflags, DISP_BF_PAUSED);
          control_vars.slaves.pauseFlags  = control_vars.pauseFlags;
          control_vars.slaves.pauseReason = PAUSE_MASTER_REQ;
        }

        // -----------------------------------------------------------
        // Flag we are initialised, so things can start happening
        // -----------------------------------------------------------
        BSET(control_vars.bitflags, DISP_BF_INITIALISED);
      }
      // -----------------------------------------------------------
      // Else, do daily maintenance, maybe reboot once a day...
      // -----------------------------------------------------------
      else
      {
        BSET(control_vars.bitflags, DISP_BF_REBOOT);
      }

      // Get & save today's sunrise/sunset for use elsewhere
      // -----------------------------------------------------------
      lightcheck.ts_sunrise = sunrise(LATITUDE, LONGITUDE, now);
      lightcheck.ts_sunset  = sunset(LATITUDE, LONGITUDE, now);

      // Set next check for just after midnight
      // Unless something weird happens there is only
      // a need to do this once a day...
      // -----------------------------------------------------------
      lightcheck.nextcheck = (now - tod) + 86401;

      // If anyone is watching, let 'em know what we know
      // (%.19s to trim off the newline)
      // -----------------------------------------------------------
      F_LOGI(true, true, LC_CYAN, "   Sunrise: %.19s", ctime(&lightcheck.ts_sunrise));
      F_LOGI(true, true, LC_CYAN, "    Sunset: %.19s", ctime(&lightcheck.ts_sunset));
      F_LOGI(true, true, LC_CYAN, "Next check: %.19s", ctime(&lightcheck.nextcheck));

      // Check for daily light display changes if we are not a slave
      // -----------------------------------------------------------
      if ( !BTST(control_vars.bitflags, DISP_BF_SLAVE) )
      {
        // Set the pattern mask (to default)
        // -----------------------------------------------------------
        uint8_t curTheme = THEME_UKRAINE;

        // Allow configuration on a per month/day basis
        // -----------------------------------------------------------
        if ( check_annual_event(tmz, true) )
        {
          ae = cur_annual_event;
        }
        else
        {
          ae = def_annual_event;
        }

        // If, after processing, our chosen theme doesn't match
        // the current one, we switch to the new one.
        // -----------------------------------------------------------
        if ( curTheme != control_vars.cur_themeID )
        {
          set_theme(curTheme, DEFAULT_DIM);
        }

        // Daily chores
        // -----------------------------------------------------------
        lights_dailies();
      }
    }

    // Check whether it is currently dark(ish) outside
    // -----------------------------------------------------------
    bool isDark = true;
    if ( lightcheck.lux_level >= 440 || (now >= lightcheck.ts_sunrise && now <= lightcheck.ts_sunset) )
    {
      isDark = false;
    }

    // Now do smoothing on the dark/light check above
    // (as smoothing starts at 0xff, first boot will always set straight away, if needed)
    // -----------------------------------------------------------
    if ( isDark != lightcheck.isdark )
    {
      // Debug helper
      if ( smoothing++ == 0 )
      {
        F_LOGI(true, true, LC_WHITE, "Smoothing day/night switch has begun");
      }
      else if ( smoothing > 60 )
      {
        lightcheck.isdark = isDark;
      }
    }
    // Reset smoothing counter
    // -----------------------------------------------------------
    else if ( smoothing )
    {
      // Debug helper
      F_LOGI(true, true, LC_WHITE, "Smoothing day/night switch has ended");

      // Reset
      smoothing = 0;
    }

    // Intermittent checks
    // *****************************************
    if ( !--minute_check )
    {
      F_LOGV(true, true, LC_GREY, "now: %ld, ts_sunrise: %ld, ts_sunset: %ld, lux: %d (isdark: %d)", now, lightcheck.ts_sunrise, lightcheck.ts_sunset, lightcheck.lux_level, lightcheck.isdark);

      if ( !BTST(control_vars.bitflags, DISP_BF_SLAVE) )
      {
        // **************************************************************************
        // * Turning the lights on                                                  *
        // * -----------------------------------------------------------------------*
        // * If we don't have a weekly day/time event, e.g. NHS Day, during covid,  *
        // * we will process our dusk/dawn schedule as normal                       *
        // **************************************************************************
        if ( check_weekly_event(tmz) == false )
        {
          process_annual(tmz, ae);
        }

        // If necessary, dim the lights after it gets dark
        // -----------------------------------------------------------
        if ( lightcheck.isdark )
        {
          // Even dimmer after 9pm
          // -----------------------------------------------------------
          if ( tmz.tm_hour >= 21 )
          {
            if ( control_vars.dim < DIM_MIN )
            {
              set_brightness(DIM_MIN);
            }
          }
          else if ( control_vars.dim < DIM_MED )
          {
            set_brightness(DIM_MED);
          }
        }
        // If we switch from dark to light, reset the brightness
        // -----------------------------------------------------------
        else if ( control_vars.dim != control_vars.theme_dim )
        {
          set_brightness(control_vars.theme_dim);
        }

        // Check for minute changes to the light display
        // -----------------------------------------------------------
        lights_sixty();
      }

      // Reset our minute check
      // -----------------------------------------------------------
      minute_check = 60 - tmz.tm_sec;
    }

    // Animate some display parameters and update any network slaves
    // -----------------------------------------------------------
    updateSync();
  }
  // If not time synced, notify anyone watching on the console.
  else
  {
    static uint8_t less_spam = 0;
    if ( (less_spam++ & 0x1f) == 0 )
    {
      F_LOGI(true, true, LC_BRIGHT_YELLOW, "Waiting for time sync..." );
    }
  }
}

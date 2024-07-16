#ifndef   __LIGHTS_H__
#define   __LIGHTS_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_timer.h>
#include <stdbool.h>
#include <esp_err.h>
#include "ws2812_driver.h"
#include "colours.h"

#define arr_len(x) (sizeof(x) / sizeof(x[0]))

typedef enum
{
  DIM_MAX,
  DIM_HIGH,
  DIM_MED,
  DIM_LOW,
  DIM_MIN,
  DIM_TOTAL
} led_brightness_t;

#define LOOP_SUMMARY_COUNT  12000                // Performance summary count

#define PIXELS              CONFIG_PIXELS
#define DEFAULT_DIM         CONFIG_DEFAULT_DIM

#define PATTERN_FIXED       1
#define PATTERN_DAILY       2
#define PATTERN_RANDOM      3

#define DEFAULT_FADE        false
#define MAX_STARS           10

#define DIR_LEFT            0
#define DIR_RIGHT           1

#define OVERLAY_TIMER       40
#define FLOOD_TIMER         40

#define MASTER_TIMEOUT      15

#define NO_PATTERN          255
// Ordered by priority. Lights need to be off during OTA, so this overrides
// user request which overrides master request and so on.
typedef enum
{
  PAUSE_NOTPROVIDED         = 0,          // 0
  PAUSE_DAYTIME             = 1 << 0,     // 1
  PAUSE_REBOOT              = 1 << 1,     // 2
  PAUSE_BOOT                = 1 << 2,     // 4
  PAUSE_SCHEDULE            = 1 << 3,     // 8
  PAUSE_EVENT               = 1 << 4,     // 16
  PAUSE_MASTER_REQ          = 1 << 5,     // 32
  PAUSE_USER_REQ            = 1 << 6,     // 64
  PAUSE_UPLOADING           = 1 << 7      // 128
} pause_event_t;

// Overlay lighting mask
#define OL_MASK_PAUSED      0x01
#define OL_MASK_DAYTIME     0x02
#define OL_MASK_NIGHTTIME   0x04

#define inc_curpos(pos,s,c)       ((++pos >= control_vars.pixel_count) ? 0 : (pos >= (s + c)) ? s : pos)
#define dec_curpos(pos,s,c)       ((pos-- == 0) ? (control_vars.pixel_count - 1) : (pos < s) ? (s + (c - 1)) : pos)
#define constrain_curpos(pos,s,c) ((pos < 0) ? (pos+control_vars.pixel_count) : (pos >= control_vars.pixel_count) ? \
                                  (pos % control_vars.pixel_count) : (pos < s) ? ((pos+c) % control_vars.pixel_count) : (pos))
#define linear2quadrant(pos,s,c)  (((s+pos) >= control_vars.pixel_count) ? ((s+pos)-control_vars.pixel_count) : (s+pos))

#define MASK_NONE           0             // Don't use this one
#define MASK_DEFAULT        1 << 0        // Had to do it.
#define MASK_MULTI          1 << 1        // Pattern can be used by other patterns, like blocky
#define MASK_EVENT          1 << 2        // Pattern can be used by an event (ie. has no colour altering functions)
#define MASK_EVENT2         1 << 3        // More reduced selection of the above event

typedef enum
{
  GLITTER,
  CONFETTI,
  FLUTTER,
  SNOW
} dazzle_mode;

typedef enum
{
  OV_ZONE_NZ                = 0x00,
  OV_ZONE_01                = 0x01,
  OV_ZONE_02                = 0x02,
  OV_ZONE_03                = 0x04,
  OV_ZONE_04                = 0x08,
  OV_ZONE_05                = 0x10,
  OV_ZONE_06                = 0x20
} overlay_zone;

typedef enum
{
  OV_MASK_PAUSED            = 0x01,       // Overlay is allowed when display is paused
  OV_MASK_RUNNING           = 0x02,       // Overlay is allowed when display is running
  OV_MASK_DAY               = 0x04,       // Overlay is allowed during the day
  OV_MASK_NIGHT             = 0x08,       // Overlay is allowed during the night
  OV_MASK_CCTV              = 0x10        // Control CCTV device
} overlay_onmask;

enum themeIdentifier
{
  THEME_DEFAULT             = 0,
  THEME_UKRAINE,
  THEME_CHRISTMAS,
  THEME_HALLOWEEN,
  THEME_WEDDING,
  THEME_MAX                 = 0xFE,
  THEME_INVALID             = 0xFF,
};

typedef enum
{
  THEME_BF_NULL             = 0,
  THEME_BF_FLASH            = 1
} theme_bitflags_t;

enum schedule { SCHED_ON, SCHED_AUTO, SCHED_OFF, SCHED_MAX };

typedef struct
{
  char                      name[15];
  CRGBPalette16            *list;
  uint8_t                   count;
  uint8_t                   themeIdentifier;
  uint8_t                   patternMask;
  uint32_t                  bitflags;
} themes_t;

typedef struct
{
  uint16_t                  start;
  uint16_t                  count;
  uint16_t                  mask;
  cRGB                      color;
} zone_params_t;

typedef struct
{
  esp_timer_handle_t        tHandle;
  zone_params_t             zone_params;
  uint8_t                   set;
  uint8_t                   state;
} overlay_t;

typedef struct
{
  uint64_t                  sTime;
  uint64_t                  eTime;
  uint8_t                   gpioPin;
  uint8_t                   isOn;
  esp_timer_handle_t        handle;
} flood_t;

typedef struct
{
  bool                      paused;               // Are the lights paused?
  uint16_t                  pauseFlags;           // Current LED display status
  uint16_t                  pauseReason;          // Reason for (un)pausing of LED display
} slave_data_t;

typedef struct
{
  bool                      paused;               // Are the lights paused?
  uint16_t                  pauseFlags;           // Current LED display status
  uint16_t                  pauseReason;          // Reason for (un)pausing of LED display
  uint32_t                  delay_us;             // Duration of a "tick"
  uint32_t                  bitflags;             //
  uint8_t                   cur_themeID;          // Current theme
  uint8_t                   cur_themeIdx;         // Current theme index
  uint8_t                   thisPalette;          //
  uint8_t                   thatPalette;          //
  uint8_t                   palIndex;             //
  uint8_t                   dim;                  // Global variable controlling LED brightness (dimmed)
  uint8_t                   palette_stretch;      //
  uint8_t                   pattern_cycle;        // ??
  uint8_t                   cur_pattern;          // Current pattern
  uint16_t                  cycle_hue;            //
  cRGB                      this_col;             //
  cRGB                      that_col;             //
  cRGB                      static_col;           //
} display_sync_t;

typedef enum
{
  DISP_BF_PAUSED            = 0b0000000000001,      // Are the lights paused?
  DISP_BF_REBOOT            = 0b0000000000010,      // When set, the appropriate code will reboot the device.
  DISP_BF_INITIALISED       = 0b0000000000100,      // Has bootup initialisation completed?
  DISP_BF_SHOW_STARS        = 0b0000000001000,      // Enhance the patterns with intermittent flashes?
  DISP_BF_SLAVE             = 0b0000000010000,      // Receive and process commands from another device?
  DISP_BF_MASTER            = 0b0000000100000,      // Send commands to other devices?
  DISP_BF_MANUAL_THEME      = 0b0000001000000,      // Whether the current theme was manually chosen.
  DISP_BF_THIS_DIR_LEFT     = 0b0000010000000,      // If set, 'this_dir' will move left
  DISP_BF_THAT_DIR_LEFT     = 0b0000100000000,      // If set, 'that_dir' will move left
} disp_bitflags_t;

#ifdef CONFIG_SHOW_STARS
#define SHOW_STARS          DISP_BF_SHOW_STARS
#else
#define SHOW_STARS          0
#endif

typedef struct
{
  uint16_t                  pauseFlags;           // Paused bit flags (zero if not paused)
  uint16_t                  lastReason;           // Last reason for pausing/unpausing the display
  uint16_t                  start;                // Virtual start
  uint16_t                  count;                // Virtual length
  uint16_t                  pixel_count;          // Total physical pixels
  uint16_t                  update_us;            // How long to update all (pixel_count) LEDs in microseconds
  uint8_t                   led_gpio_pin;         // WS2812 output pin
  uint8_t                   light_gpio_pin;       // Security output pin
  uint8_t                   schedule;             // SCHED_ON | SCHED_AUTO | SCHED_OFF
  uint8_t                   num_overlays;         // Number of overlays
  uint8_t                   num_themes;           // Number of themes
  uint32_t                  bitflags;             //
  uint32_t                  delay_us;             // Duration of a "tick"
  uint8_t                   master_alive;         // Timeout when we last received a command from the master (to enter autonomous mode).
  uint8_t                   delay_ticks;          // Modified by each pattern
  uint8_t                   cur_themeID;          // Current theme
  uint8_t                   cur_themeIdx;         // Current theme index
  uint8_t                   thisPalette;          //
  uint8_t                   thatPalette;          //
  uint8_t                   palIndex;             //
  uint8_t                   theme_dim;            // The brightness level specified by the current theme.
  uint8_t                   dim;                  // Global variable controlling LED brightness (dimmed)
  uint8_t                   palette_stretch;      //
  uint8_t                   pattern_cycle;        // ??
  uint8_t                   cur_pattern;          // Current pattern
  uint8_t                   pre_pattern;          // Previous pattern
  uint8_t                   num_patterns;         // Total number of patterns
  uint8_t                   cycle_hue;            //
  cRGB                      this_col;             //
  cRGB                      that_col;             //
  display_sync_t            shared;               // Settings shared with minions.
  cRGB                      static_col;           //
  slave_data_t              slaves;               // Vars for controlling slaves that differ to the master
} controlvars_t;

typedef struct
{
  char                      name[24];
  void                      (*pointer)(uint16_t, uint16_t, uint8_t, controlvars_t *);
  bool                      enabled;
  uint8_t                   num_stars;
  dazzle_mode               star_mode;
  uint16_t                  mask;
} patterns_t;

extern flood_t              flood[];
extern overlay_t            overlay[];
extern patterns_t           patterns[];
extern cRGB                 basicColors[];
extern cRGB                *rgbBuffer;

extern CRGBPalette16       *curPalette;
extern cRGB                *outBuffer;
extern controlvars_t        control_vars;

bool        check_valid_input (uint8_t pin);
bool        check_valid_output (uint8_t pin);
esp_err_t   lights_init (void);
void        lights_task (void *pvParameters);

void        setPixelRGB (uint16_t n, cRGB c, bool saveColor, uint8_t ldim);
void        setPixelHSV (uint16_t n, cHSV c, bool saveColor, uint8_t ldim);

void        restorePaletteAll (void);
void        show (void);

cRGB        getPixelRGB (uint16_t n);
cHSV        getPixelHSV (uint16_t n);

bool        fadeToBlack (uint16_t i, uint8_t fadeValue);

cRGB        wheel (unsigned int pos);
cRGB        colorfade (cRGB pixel);
cRGB        hsv2rgb (cHSV hsv);
cHSV        rgb2hsv (cRGB rgb);

cHSV        blendHSV (cHSV a, cHSV b, float c);
cRGB        blendRGB (cRGB c1, cRGB c2);

cHSV        hsvFromPalette (const CRGBPalette16 pal, uint8_t index, uint8_t brightness);
cRGB        rgbFromPalette (const CRGBPalette16 pal, uint8_t index);

// Functions requiring start, count arguments
void        FillLEDsFromPaletteColors (uint16_t startIndex, uint16_t startPos, uint16_t count);

void        rgbFillSolid (uint16_t startPos, uint16_t count, cRGB colour, bool save, uint8_t ldim);
void        hsvFillSolid (uint16_t startPos, uint16_t count, cHSV colour, bool save);
bool        rgbFadeAll (uint16_t startPos, uint16_t count, uint8_t fadeValue);
bool        hsvFadeAll (uint16_t startPos, uint16_t count, uint8_t fadeValue);

void        restorePalette (uint16_t startPos, uint16_t count);
bool        logicalShift (uint16_t startPos, uint16_t count, bool dir, bool rot);

//
void        printPausedFlags (const char *prefix, uint16_t bitflags);
void        updateSync(void);
void        lightsPause (uint16_t reason);
void        lightsUnpause (uint16_t reason, bool clearOnly);
bool        lightsPausedReason (uint16_t reason);
bool        lightsPaused (void);
void        showColourPreview (cRGB colour);

void        initReboot ();
void        test_rgb2hsv (cRGB colour);

void        lights_dailies (void);
void        lights_sixty (void);

esp_err_t   set_theme (uint8_t theme, uint8_t dim = 0xFF);
bool        set_pattern (uint16_t pattern);
int16_t     getPatternByName (const char *name, bool sel);

bool        set_brightness(uint8_t level);
bool        set_loop_delay(uint32_t delay_us);

uint8_t     get_theme_idx_by_id (uint8_t id);
const char *get_theme_name (uint8_t theme);
uint8_t     get_theme_by_name (const char *name, bool sel);
bool        switch_pattern (void);


#endif

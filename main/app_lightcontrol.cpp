


#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <driver/gptimer.h>
#include <driver/gpio.h>

#include <esp_task_wdt.h>
#include <esp_err.h>

#include <sys/time.h>
#include <soc/rtc.h>
#include <esp_pm.h>

#include <time.h>

#include "app_main.h"
#include "ws2812_driver.h"
#include "app_lightcontrol.h"
#include "patterns.h"
#include "app_sntp.h"
#include "app_mqtt.h"
#include "app_utils.h"
#include "app_timer.h"
#include "app_flash.h"

// ------------------------------------------------------
// This is the minium runtime.
// Writing all bits for a single LED takes about 30us.
// ------------------------------------------------------
// Warning: 30us * 492 = 14760 us (manufacturer settings)
//                     = 13587 us (optimized)
// ------------------------------------------------------
#define LED_LOOP_DELAY_US     CONFIG_LED_LOOP_US
// ------------------------------------------------------
//#define CONFIG_DEBUG_PAUSE    true

// Force current test pattern if we are debugging
// (avoid accidentally forcing patterns on other devices)
#if defined(CONFIG_AEOLIAN_DEBUG_DEV)
//#define FORCE_PATTERN       "Police Lights"
//#define FORCE_PATTERN       "Two Sin"
//#define FORCE_PATTERN       "Pride"
//#define FORCE_PATTERN       "Old School 2"
#endif /* CONFIG_AEOLIAN_DEBUG_DEV */

extern uint16_t led_debug_cnt;
extern uint32_t led_write_count;
extern uint64_t led_accum_time;

void set_thisCol (void);
void set_thatCol (void);

/*
// Generate an LED gamma-correction table for Arduino sketches.
// Written in Processing (www.processing.org), NOT for Arduino!
// Copy-and-paste the program's output into an Arduino sketch.

float gamma   = 2.8; // Correction factor
int   max_in  = 255, // Top end of INPUT range
      max_out = 255; // Top end of OUTPUT range

void setup() {
  print("const uint8_t PROGMEM gamma[] = {");
  for(int i=0; i<=max_in; i++) {
    if(i > 0) print(',');
    if((i & 15) == 0) print("\n  ");
    System.out.format("%3d",
      (int)(pow((float)i / (float)max_in, gamma) * max_out + 0.5));
  }
  println(" };");
  exit();
}
*/
#define MIN_DIM_LEVEL   35

const uint8_t ICACHE_RODATA_ATTR gamma8[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255};

patterns_t patterns[] = {
/******************************************************************************************/
/* Human Name               Command             Enabled   Stars    Star Mode     MASK     */
/******************************************************************************************/
  {"Blocky",                blocky,             false,    0,       GLITTER,      0},                              // Unsure
  {"Bouncing Balls",        bouncingballs,      false,    0,       GLITTER,      0},                              // Shite
  {"Candy Corn",            candycorn,          false,    0,       GLITTER,      0},
  {"Circular",              circular,           true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Colour Waves",          color_waves,        true,     3,       FLUTTER,      MASK_DEFAULT | MASK_MULTI},
  {"Colour Wipe 1",         colorwipe_1,        true,     3,       FLUTTER,      MASK_DEFAULT | MASK_MULTI},
  {"Colour Wipe 2",         colorwipe_2,        true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Daily Colour",          daily_colour,       true,     8,       FLUTTER,      MASK_DEFAULT | MASK_MULTI},
  {"Disco Strobe",          discostrobe,        false,    0,       GLITTER,      MASK_DEFAULT},
  {"Double Chaser",         doubleChaser,       true,     3,       FLUTTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT},
  {"Fade in & Out",         fadeinandout,       true,     3,       FLUTTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Juggle",                juggle,             false,    0,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Lone Wolf",             loneWolf,           true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},      // Broken
  {"Matrix",                matrix,             true,     0,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Meteor Rain",           meteorRain,         true,     0,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Null",                  null_pattern,       false,    0,       GLITTER,      0},
  {"Old School 1",          oldschool,          false,    3,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Old School 2",          oldschoolRotate,    true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"One Sin",               one_sin_init,       true,     0,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT},
  {"Out of Time",           out_of_time,        true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Palette Scroll",        palettescroll,      true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Plasma",                plasma,             true,     1,       FLUTTER,      MASK_DEFAULT | MASK_MULTI},
  {"Police Lights",         police_lights,      false,    0,       GLITTER,      0},
  {"Pride",                 pride,              true,     0,       GLITTER,      MASK_DEFAULT},
  {"Rachels",               rachels,            false,    0,       GLITTER,      0},                                // Broken
  {"Rainbow Cycle",         rainbow_one,        true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Rainbow Strip",         rainbow_two,        true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Scanner",               larsenScanner,      true,     0,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT},
  {"Side Rain",             side_rain,          true,     0,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Test",                  test_pattern,       false,    0,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Traditional",           traditional,        true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Twinkle Map",           twinklemappixels,   true,     2,       GLITTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Two Col Sin",           two_col_init,       true,     3,       CONFETTI,     MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},
  {"Two Sin",               two_sin_init,       true,     3,       GLITTER,      MASK_DEFAULT | MASK_MULTI},
  {"Undulate",              undulate,           true,     3,       FLUTTER,      MASK_DEFAULT | MASK_MULTI | MASK_EVENT | MASK_EVENT2},  // Shows red leds on Ukraine and shouldn't
};

#define NUM_PATTERNS ((sizeof(patterns) / sizeof(patterns_t)) -1)
#define RAND_PATTERN patterns[prng() % NUM_PATTERNS]

CRGBPalette16 *curPalette = NULL;
uint8_t paletteSize = 0;

cRGB *rgbBuffer = NULL;
cRGB *outBuffer = NULL;

// Our overlay for when motion is detected in a specific quadrant
#if defined (CONFIG_AEOLIAN_DEV_CARAVAN)
overlay_t overlay[] = {
  {NULL,  { 16,  44,  0,   MediumWhite},  0,  false},
  {NULL,  { 60, 154,  0,   MediumWhite},  0,  false},
  {NULL,  {226,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {257, 147,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false}};
#elif defined (CONFIG_AEOLIAN_DEV_DEBUG)
overlay_t overlay[] = {
  {NULL,  { 45,   2,  0,   MediumWhite},  0,  false},
  {NULL,  { 43,   2,  0,   MediumWhite},  0,  false},
  {NULL,  { 41,   2,  0,   MediumWhite},  0,  false},
  {NULL,  { 38,   2,  0,   MediumWhite},  0,  false},
  {NULL,  { 32,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 33,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 20,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 21,   1,  0,   MediumWhite},  0,  false}};
#elif defined (CONFIG_AEOLIAN_DEV_OLIMEX)
overlay_t overlay[] = {
  {NULL,  {  0,   3,  0,   MediumWhite},  0,  false},
  {NULL,  {  3,   3,  0,   MediumWhite},  0,  false},
  {NULL,  {  6,   3,  0,   MediumWhite},  0,  false},
  {NULL,  {  9,   3,  0,   MediumWhite},  0,  false},
  {NULL,  { 10,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 11,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 12,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 13,   1,  0,   MediumWhite},  0,  false}};
#elif defined (CONFIG_AEOLIAN_DEV_TTGO)
overlay_t overlay[] = {
  {NULL,  {  0,   3,  0,   MediumWhite},  0,  false},
  {NULL,  {  3,   3,  0,   MediumWhite},  0,  false},
  {NULL,  {  9,   3,  0,   MediumWhite},  0,  false},
  {NULL,  { 12,   3,  0,   MediumWhite},  0,  false},
  {NULL,  { 17,   1,  0,   MediumWhite},  0,  false},
  {NULL,  { 18,   1,  0,   MediumWhite},  0,  false},
  {NULL,  {  7,   1,  0,   MediumWhite},  0,  false},
  {NULL,  {  8,   1,  0,   MediumWhite},  0,  false}};
#elif defined (CONFIG_AEOLIAN_DEV_WORKSHOP)
overlay_t overlay[] = {
  {NULL,  {367,  84,  0,   MediumWhite},  0,  false},
  {NULL,  { 67, 110,  0,   MediumWhite},  0,  false},
  {NULL,  {177,  80,  0,   MediumWhite},  0,  false},
  {NULL,  {257, 110,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false}};
#elif defined (CONFIG_AEOLIAN_DEV_S3_MATRIX)
overlay_t overlay[] = {
  {NULL,  {  0,  16,  0,   MediumWhite},  0,  false},
  {NULL,  { 16,  16,  0,   MediumWhite},  0,  false},
  {NULL,  { 32,  16,  0,   MediumWhite},  0,  false},
  {NULL,  { 48,  16,  0,   MediumWhite},  0,  false},
  {NULL,  {  1,   1,  0,   MediumWhite},  0,  false},
  {NULL,  {  3,   1,  0,   MediumWhite},  0,  false},
  {NULL,  {  6,   2,  0,   MediumWhite},  0,  false},
  {NULL,  {  8,   2,  0,   MediumWhite},  0,  false}};
#else
overlay_t overlay[] = {
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  0,   0,  0,   MediumWhite},  0,  false},
  {NULL,  {  10,  1,  0,   MediumWhite},  0,  false},
  {NULL,  {  11,  1,  0,   MediumWhite},  0,  false}};
#endif
#define NUM_OVERLAYS  (sizeof(overlay) / sizeof(overlay_t))

flood_t flood[1] = {{}};

// ***********************************************************************************
// * Basic colours
// ***********************************************************************************
cRGB basicColors[] = {
  {0xFF, 0x00, 0x00}, {0xFF, 0x88, 0x00}, {0xFF, 0xBB, 0x00}, {0xEE, 0xFF, 0x00}, {0x44, 0xFF, 0x00}, {0x00, 0xFF, 0x99}, {0x00, 0xFF, 0xEE},
  {0x00, 0xDD, 0xFF}, {0x00, 0xAA, 0xFF}, {0x00, 0x66, 0xFF}, {0x88, 0x00, 0xFF}, {0xFF, 0x00, 0xDD}, {0xFF, 0x00, 0x88}, {0xFF, 0x00, 0x44},
};
#define RAND_BCOLOUR basicColors[prng() % arr_len(basicColors)]

// ******************************************************************
// * Wedding colours and palettes
// ******************************************************************
CRGBPalette16 pWedding[] = {
  {{White, Magenta, White, Magenta, White, Magenta, White, Magenta, White, Magenta, White, Magenta, White, Magenta, White, Magenta}},
  {{Magenta, Magenta, Black, White, White, Magenta, Magenta, Black, White, White, Magenta, Magenta, Black, White, White, Black}},
  {{White, Black, Magenta, White, Black, Magenta, White, Black, Magenta, White, Black, Magenta, White, Black, Magenta, White}}
};
#define WEDDING_TOP arr_len(pWedding)
#define RAND_WEDDING pWedding[random8(WEDDING_TOP)]

// ******************************************************************
// * Halloween colours and palettes
// ******************************************************************
#define hBlack        {0x00, 0x00, 0x00}
#define hOrange       {0xFF, 0x45, 0x00}
#define hPurple       {0xF0, 0x00, 0xF0}
#define hGreen        {0x00, 0xA0, 0x00}
#define hYellow       {0xFF, 0xB0, 0x00}
#define hRed          {0xE0, 0x00, 0x00}
CRGBPalette16 pHalloween[] = {
  {{hPurple, hRed, hPurple, hRed, hPurple, hRed, hPurple, hRed, hPurple, hRed, hPurple, hRed, hPurple, hRed, hPurple, hRed}},
  {{hPurple, hOrange, hPurple, hOrange, hPurple, hOrange, hPurple, hOrange, hPurple, hOrange, hPurple, hOrange, hPurple, hOrange, hPurple, hOrange}},
  {{hPurple, hYellow, hPurple, hYellow, hPurple, hYellow, hPurple, hYellow, hPurple, hYellow, hPurple, hYellow, hPurple, hYellow, hPurple, hYellow}},
  {{hPurple, hGreen, hPurple, hGreen, hPurple, hGreen, hPurple, hGreen, hPurple, hGreen, hPurple, hGreen, hPurple, hGreen, hPurple, hGreen}},
  {{hOrange, hPurple, hGreen, hYellow, hRed, hOrange, hPurple, hGreen, hYellow, hRed, hOrange, hPurple, hGreen, hYellow, hRed, hBlack}},
};
#define HALLOWEEN_TOP arr_len(pHalloween)
#define RAND_HALLOWEEN pHalloween[random8(HALLOWEEN_TOP)]

// ******************************************************************
// * Christmas colours and palettes
// ******************************************************************
#define cBlack        {0x00, 0x00, 0x00}
#define cGreen        {0x00, 0xF0, 0x00}
#define cYellow       {0xF8, 0xA0, 0x00}
#define cRed          {0xF0, 0x00, 0x00}
#define cBlue         {0x00, 0x00, 0xF0}
#define cWhite        {0xF0, 0xF0, 0xF0}
CRGBPalette16 pChristmas[] = {
  {{cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen}},
  {{cRed, cGreen, cYellow, cBlue, cRed, cGreen, cYellow, cBlue,cRed, cGreen, cYellow, cBlue,cRed, cGreen, cYellow, cBlue,}},
  {{cRed, cBlue, cGreen, cYellow, cRed, cBlue, cGreen, cYellow, cRed, cBlue, cGreen, cYellow, cRed, cBlue, cGreen, cYellow}},
  {{cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed, cGreen, cRed}},
};
#define CHRISTMAS_TOP arr_len(pChristmas)
#define RAND_CHRISTMAS pChristmas[random8(CHRISTMAS_TOP)]

// ******************************************************************
// * UKRAINE colours and palettes
// ******************************************************************
CRGBPalette16 pUkraine[] = {
  {{{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},
    {0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0xFF,0xD7,0x00}}},
  {{{0x00,0x57,0xBB},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0xFF,0xD7,0x00},
    {0x00,0x57,0xBB},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0xFF,0xD7,0x00},{0x00,0x57,0xBB},{0x00,0x57,0xBB},{0xFF,0xD7,0x00},{0xFF,0xD7,0x00}}},
};
#define UKRAINE_TOP arr_len(pUkraine)
#define RAND_UKRAINE pUkraine[random8(UKRAINE_TOP)]

// ******************************************************************
// * Default colours and palettes
// ******************************************************************
CRGBPalette16 pDefault[] = {
  {{Aqua, Yellow, White, SpringGreen, Red, OrangeRed, Lime, LightCyan, Blue, Green, Teal, Magenta, Cyan, Navy, Indigo, Gold}},
  {{Red, Yellow, Magenta, OrangeRed, LightCyan, White, Blue, Green, Red, Yellow, Magenta, OrangeRed, LightCyan, White, Blue, Green}}
};
#define DEFAULT_TOP arr_len(pDefault)
#define RAND_DEFAULT pDefault[random8(DEFAULT_TOP)]

themes_t themes[] = {
/********************************************************************************************************/
/*   Human Name     Palette       Count           Theme               Patterns          Bit Flags       */
/********************************************************************************************************/
  { "Default",      pDefault,     DEFAULT_TOP,    THEME_DEFAULT,      MASK_DEFAULT,     THEME_BF_FLASH},
  { "Ukraine",      pUkraine,     UKRAINE_TOP,    THEME_UKRAINE,      MASK_EVENT,       0},
  { "Christmas",    pChristmas,   CHRISTMAS_TOP,  THEME_CHRISTMAS,    MASK_EVENT2,      THEME_BF_FLASH},
  { "Wedding",      pWedding,     WEDDING_TOP,    THEME_WEDDING,      MASK_EVENT2,      THEME_BF_FLASH},
  { "Halloween",    pHalloween,   HALLOWEEN_TOP,  THEME_HALLOWEEN,    MASK_EVENT2,      THEME_BF_FLASH},
};
#define NUM_THEMES  (sizeof(themes) / sizeof(themes_t))

// Which palette to use by default
#define DEFAULT_THEME     THEME_DEFAULT

#pragma GCC diagnostic push                                     // Save the current warning state
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // Disable missing-field-initilizers
controlvars_t control_vars = {
  .pixel_count      = PIXELS,
  .led_gpio_pin     = WS2812_PIN,
  .light_gpio_pin   = LIGHT_PIN,
  .num_overlays     = NUM_OVERLAYS,
  .num_themes       = NUM_THEMES,
  .bitflags         = DISP_BF_PAUSED | SHOW_STARS,
  .master_alive     = 0,
  .delay_ticks      = 1,
  .cur_themeID      = 0xFF,
  .cur_themeIdx     = THEME_MAX,
  .theme_dim        = DEFAULT_DIM,
  .dim              = DEFAULT_DIM,
  .palette_stretch  = 1,
  .pattern_cycle    = PATTERN_DAILY,
  .cur_pattern      = NO_PATTERN,
  .pre_pattern      = NO_PATTERN,
  .num_patterns     = NUM_PATTERNS,
  .cycle_hue        = 0,
  .this_col         = Black,
  .that_col         = Black,
  .static_col       = Black,
};
#pragma GCC diagnostic pop                                      // Restore previous default behaviour

typedef struct
{
  uint16_t delay;
  cRGB colour;
} colPreview_t;
colPreview_t previewColour = {};

const uint8_t valid_input_pins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
const uint8_t valid_output_pins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
bool check_valid_input (uint8_t pin)
{
  bool valid = false;

  for ( uint8_t x = 0; x < (sizeof (valid_input_pins) / sizeof (uint8_t)); x++ )
  {
    if ( pin == valid_input_pins[x] )
    {
      valid = true;
      break;
    }
  }
  return valid;
}
bool check_valid_output (uint8_t pin)
{
  bool valid = false;

  for ( uint8_t x = 0; x < (sizeof (valid_output_pins) / sizeof (uint8_t)); x++ )
  {
    if ( pin == valid_output_pins[x] )
    {
      valid = true;
      break;
    }
  }
  return valid;
}

// **********************************************************************
// Toggle PM on/off (if user has enabled it)
// PM doesn't really work well with a fast, responsive LED display, so
// we will toggle it on and off when required.
// **********************************************************************
#ifdef CONFIG_PM_ENABLE
IRAM_ATTR void set_pm(bool switch_on)
{
  F_LOGI(true, true, LC_BRIGHT_BLUE, "set_pm(%s) - current cpu MHz: %d", switch_on?"true":"false", get_cpu_mhz());

  int min_freq = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
  if ( switch_on == true )
  {
    min_freq = (int) rtc_clk_xtal_freq_get();
  }

  esp_pm_config_t pm_config = {
    .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
    .min_freq_mhz = min_freq,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    .light_sleep_enable = true
#else
    .light_sleep_enable = false
#endif
  };
  esp_err_t err = esp_pm_configure(&pm_config);
  if ( err != ESP_OK )
  {
    F_LOGE(true, true, LC_YELLOW, "Problem running esp_pm_configure()");
  }
}
#endif /* CONFIG_PM_ENABLE */

// **********************************************************************
// Initialise the LED display task
// **********************************************************************
#define BLANK_COUNT     2
esp_err_t lights_init(void)
{
  esp_err_t err = ESP_OK;

  // Initialise the LED system
  // *****************************************
  ws2812_init ((gpio_num_t)control_vars.led_gpio_pin, control_vars.pixel_count);
  gpio_set_drive_capability ((gpio_num_t)control_vars.led_gpio_pin, GPIO_DRIVE_CAP_1);

  // Configure our working buffer(s)
  // *****************************************
  if ( (rgbBuffer = (cRGB *)pvPortMalloc (sizeof (cRGB) * control_vars.pixel_count)) == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'rgbBuffer'", (sizeof (cRGB) * control_vars.pixel_count));
  }
  else if ( (outBuffer = (cRGB *)pvPortMalloc (sizeof (cRGB) * control_vars.pixel_count)) == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'outBuffer'", (sizeof (cRGB) * control_vars.pixel_count));
  }
  else
  {
    // Set the starting palette list and count of palettes
    // *****************************************
    if ( set_theme(DEFAULT_THEME) == ESP_OK )
    {
      control_vars.thisPalette = (prng () % paletteSize);
      control_vars.thatPalette = (prng () % paletteSize);
      F_LOGV(true, true, LC_GREY, "thisPalette = %d, thatPalette = %d", control_vars.thisPalette, control_vars.thatPalette);

      control_vars.this_col = rgbFromPalette(curPalette[control_vars.thisPalette], control_vars.palIndex);
      control_vars.that_col = rgbFromPalette(curPalette[control_vars.thatPalette], control_vars.palIndex);

      control_vars.delay_us = LED_LOOP_DELAY_US;
    }
  }

  return err;
}

// ******************************************************************
// Check mask against day/night/display settings
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
bool show_overlay (uint16_t onMask)
#else
IRAM_ATTR bool show_overlay (uint16_t onMask)
#endif
{
  if ( BTST(control_vars.bitflags, DISP_BF_PAUSED) )
  {
    if ( !BTST (onMask, OV_MASK_PAUSED) )
    {
      return false;
    }
  }
  else if ( !BTST (onMask, OV_MASK_RUNNING) )
  {
    return false;
  }

  if ( lightcheck.isdark )
  {
    if ( !BTST (onMask, OV_MASK_NIGHT) )
    {
      return false;
    }
  }
  else if ( !BTST (onMask, OV_MASK_DAY) )
  {
    return false;
  }

  return true;
}

/* ************************************************************************************* */
/* *    Lights Main Task                                                                 */
/* ************************************************************************************* */
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void lights_task (void *pvParameters)
#else
IRAM_ATTR void lights_task (void *pvParameters)
#endif
{
  uint32_t notify_value;                   // Used by XNotifyWait
  // Used to calculate whole loop time, including forced delay
  uint32_t loops           = 0;            // Count of actual loops, including when code wasn't run.
  uint64_t loop_start      = 0;            // Loop start time, in milliseconds
  uint64_t loop_tot        = 0;            // Cumulative time of loop, including delay
  // Used to calculate the execution time of the loop.
  uint64_t code_start      = 0;            // Time loop began (doubles up as last_loop_start_time)
  uint32_t code_tot        = 0;            // Cumaltive time of all loops
  // Used to calculate the execution time of an individual pattern code
  uint32_t pattern_loops   = 0;            // Only count times we run a pattern
  uint64_t pattern_start   = 0;            // Time pattern code began
  uint32_t pattern_tot     = 0;            // Cumaltive time running code
  // Work saving stats
  uint32_t updates         = 0;            // Loops with an update
  uint32_t skips           = 0;            // Loops without an update
  // Reduce our work load
  uint16_t blank_leds      = BLANK_COUNT;  // Doesn't always blank first shot
  bool     update_leds     = false;

  // Add ourselves to the task watchdog list
  // ------------------------------------------------------
#if CONFIG_APP_TWDT
  CHECK_ERROR_CODE (esp_task_wdt_add(NULL), ESP_OK);
  CHECK_ERROR_CODE (esp_task_wdt_status(NULL), ESP_OK);
#endif

  // Wait for our pattern to be set
  while ( control_vars.cur_pattern == 255 )
  {
    // Reset the watchdog
    // ------------------------------------------------------
#if CONFIG_APP_TWDT
    CHECK_ERROR_CODE (esp_task_wdt_reset(), ESP_OK);
#endif
    //yield_delay_us (500);
    delay_ms(500);
  }

  // Enable our ticker
  timer_init_with_taskHandle (TIMER_GROUP_0, TIMER_0, TIMER_DIVIDER_US, control_vars.delay_us, xTaskGetCurrentTaskHandle ());

  // Zero our buffers.
  // ------------------------------------------------------
  memset (rgbBuffer, 0, sizeof (cRGB) * control_vars.pixel_count);

  // The main light loop
  do
  {
    // Wait for the timer before proceeding.
    xTaskNotifyWait(0, 0xFFFFFFFF, &notify_value, portMAX_DELAY);

    // Get the current start time in milliseconds
    loop_start = esp_timer_get_time();
    if ( code_start )
    {
      loop_tot += (loop_start - code_start);
    }

    // Use this for the start of code execution and as a reference to the
    // start of the previous loop/
    code_start = loop_start;

    // Clear our output buffer
    // ------------------------------------------------------
    memset (outBuffer, 0, sizeof (cRGB) * control_vars.pixel_count);

    if ( previewColour.delay )
    {
      rgbFillSolid (0, 5, previewColour.colour, false, 0);
      previewColour.delay--;
      update_leds = true;
    }
    // If not paused, run desired display
    else if ( !BTST(control_vars.bitflags, DISP_BF_PAUSED) )
    {
      // ------------------------------------------------------
      // ------------------------------------------------------
      if ( control_vars.delay_ticks )
      {
        control_vars.delay_ticks--;
        restorePaletteAll();
      }
      else
      {
        // Flag we need to update leds
        update_leds = true;

        // Display the current pattern
        // ------------------------------------------------------
        // Get current milliseconds
        pattern_loops++;
        pattern_start = esp_timer_get_time();
        ((*patterns[control_vars.cur_pattern].pointer) (0, control_vars.pixel_count, 0, &control_vars));
        pattern_tot += (esp_timer_get_time() - pattern_start);

#if defined (FAST_HSV)
        ++control_vars.cycle_hue;
#else   // FAST_HSV
        if ( ++control_vars.cycle_hue >= 360 )
        {
          control_vars.cycle_hue = 0;
        }
#endif  // FAST_HSV
        // Save this (previously run) pattern
        control_vars.pre_pattern = control_vars.cur_pattern;
      }

      // Add stars (if desired)
      // ------------------------------------------------------
      if ( (BTST(themes[control_vars.cur_themeIdx].bitflags, THEME_BF_FLASH) && BTST(control_vars.bitflags, DISP_BF_SHOW_STARS)) && patterns[control_vars.cur_pattern].num_stars )
      {
        // Show some twinkles
        if ( dazzle(&control_vars, patterns[control_vars.cur_pattern].num_stars, patterns[control_vars.cur_pattern].star_mode) )
        {
          // Flag we need to update leds
          update_leds = true;
        }
      }
    }
// -------------------------------------------------------------------------------------------------
// --  Some ranbdom light flickering to simulate a dying bulb or loosa connection in the breeze.  --
// -------------------------------------------------------------------------------------------------
#if defined (CONFIG_AEOLIAN_DEV_WORKSHOP)
    else if ( lightcheck.isdark )
//#if defined (CONFIG_AEOLIAN_DEV_OLIMEX)
    {
      static uint32_t dt = 5000;
      static uint16_t cp = 0;

      // Randomly turn all lights white, as if we have a loose connection
      if ( dt == 0 )
      {
        //F_LOGW(true, true, LC_BRIGHT_MAGENTA, "Randomness");

        update_leds = true;
        setPixelRGB (cp++, CRGBMediumWhite, false, 2);
        if ( cp >= control_vars.pixel_count )
        {
          cp = 0;
          dt = (prng () % 500000) + 500;
        }
      }
      else
      {
        dt--;
      }
      static uint8_t flkr_pixels[] = {3, 31, 41};
      static uint16_t flkr_delay[][2] = {{0, 0}, {0, 0}, {0, 0}};
      static uint16_t cfp = 0;
#define NUM_FLKR_PIXELS  sizeof (flkr_pixels) / sizeof (uint8_t)
      for ( uint16_t cfp = 0; cfp < NUM_FLKR_PIXELS; cfp++ )
      {
        if ( flkr_delay[cfp][0] > 0 )
        {
          cRGB y = getPixelRGB (flkr_pixels[cfp]);
          uint8_t cy = y.r;
          if ( y.r > 230 )
          {
            y.r -= 1;
            y.g -= 1;
            y.b -= 1;
          }
          else if ( y.r && y.r < 30 )
          {
            y.r -= 1;
            y.g -= 1;
            y.b -= 1;
          }
          else
          {
            if ( !y.r && (prng () % 25) < 22 )
            {
              // Do nothing
            }
            else if ( (prng () % 50) <= 10 )
            {
              y.r = prng ();
              y.g = y.r;
              y.b = y.r;
            }
          }
          // Only update on change
          if ( y.r != cy )
          {
            update_leds = true;
            setPixelRGB (flkr_pixels[cfp], y, true, 2);
          }

          // Decrement our 'noisy' period
          flkr_delay[cfp][0]--;
        }
        else if ( flkr_delay[cfp][1] > 0 )
        {
          // Decrement our 'quiet' period
          flkr_delay[cfp][1]--;
        }
        // Create periods of noise and quiesence.
        else //if ( flk_delay[cfp][0] == 0 && flk_delay[cfp][1] == 0 )
        {
          flkr_delay[cfp][0] = (prng() % 400) + 50;
          flkr_delay[cfp][1] = (prng() % 3000) + 50;
        }
      }
    }
#endif
// -------------------------------------------------------------------------------------------------

    // Any overlays after all the animations have been done
    // ------------------------------------------------------
    if ( !BTST(control_vars.pauseFlags, PAUSE_UPLOADING) )
    {
      for ( int c = 0; c < control_vars.num_overlays; c++ )
      {
        if ( overlay[c].set && overlay[c].zone_params.count )
        {
          if ( show_overlay (overlay[c].zone_params.mask) )
          {
            update_leds = true;
#if defined (CONFIG_AEOLIAN_RGB_ORDER)
            rgbFillSolid(overlay[c].zone_params.start, overlay[c].zone_params.count, overlay[c].zone_params.color, false, 0);
#else
            rgbFillSolid(overlay[c].zone_params.start, overlay[c].zone_params.count, overlay[c].zone_params.color, false, 4);
#endif
          }
        }
      }
    }

    // Send our LED buffer to the WS2812 light string
    // ------------------------------------------------------
#ifdef CONFIG_PM_ENABLE
    static bool pm_enabled    = true;  // deault to true, so we always turn it off first
    static uint16_t pm_delay  = 0;     // Delay for turning PM off
#endif /* CONFIG_PM_ENABLE */

    if ( update_leds )
    {
#ifdef CONFIG_PM_ENABLE
      if ( pm_enabled == true )
      {
        set_pm(false);
        pm_enabled = false;
        pm_delay = 0;
      }
      else if ( pm_delay <= 1 )
      {
        // Do nothing for now
        pm_delay++;
      }
      else
      {
#endif /* CONFIG_PM_ENABLE */
        // Update LEDs
        ws2812_setColors (control_vars.pixel_count, outBuffer);

        // Reset update boolean
        update_leds = false;
        blank_leds  = BLANK_COUNT;
        updates++;
#ifdef CONFIG_PM_ENABLE
        pm_delay = 0;    // Delay turning PM back on to stop flicker
      }
#endif /* CONFIG_PM_ENABLE */
    }
    else if ( BTST(control_vars.bitflags, DISP_BF_PAUSED) && blank_leds )
    {
      //printf("blank: %d\n", led_debug_cnt);
      // Clear our output buffer
      // ------------------------------------------------------
      memset (outBuffer, 0, sizeof (cRGB) * control_vars.pixel_count);
      ws2812_setColors (control_vars.pixel_count, outBuffer);
      blank_leds--;
      updates++;
#ifdef CONFIG_PM_ENABLE
      pm_delay = 0;    // Delay turning PM back on to stop flicker
#endif /* CONFIG_PM_ENABLE */
    }
    else
    {
#ifdef CONFIG_PM_ENABLE
      // Delay turning PM back on, so we aren't constantly swapping and changing
      // and causing disruption to the display.
      if ( pm_delay < 400 )
      {
        pm_delay++;
      }
      else if ( pm_enabled == false )
      {
        set_pm(true);
        pm_enabled = true;
        pm_delay = 0;
      }
      else
#endif /* CONFIG_PM_ENABLE */
      skips++;
    }

    // ------------------------------------------------------
    // Calculate and display some stats to help tune settings.
    // ------------------------------------------------------
    code_tot += (esp_timer_get_time() - code_start);

    if ( ++loops >= LOOP_SUMMARY_COUNT )
    {
      uint32_t write_ave = 0;
      if ( led_write_count )
      {
        write_ave          = (uint32_t)(led_accum_time / led_write_count);
        led_accum_time     = 0;
        led_write_count    = 0;
      }

      uint32_t loop_ave = loop_tot / loops;
      //uint32_t code_ave = (code_tot - pattern_tot) / loops;
      uint32_t pattern_ave = 0;
      if ( pattern_loops )
      {
        pattern_ave = pattern_tot / pattern_loops;
      }
      F_LOGI(true, true, LC_GREY, "%d loops, %dms per loop. Pattern called %d times(%dus ave). %d physical writes(%dms ave)", loops, (loop_ave / 1000), pattern_loops, pattern_ave, updates, (write_ave / 1000));

      loops         = 0;
      loop_tot      = 0;
      code_tot      = 0;
      pattern_loops = 0;
      pattern_tot   = 0;
      updates       = 0;
      skips         = 0;
    }

    // Reset the watchdog
    // ------------------------------------------------------
#if CONFIG_APP_TWDT
    CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
#endif
  }
  while ( true );
}

// ******************************************************************
// ******************************************************************
void showColourPreview (cRGB colour)
{
  previewColour.colour = colour;
  previewColour.delay = 300;
}

/* ************************************************************************************* */
/* *       Set a Pixel                                                                 * */
/* ************************************************************************************* */
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void setPixelRGB (uint16_t n, cRGB rgb, bool saveColor, uint8_t ldim)
#else
IRAM_ATTR void setPixelRGB (uint16_t n, cRGB rgb, bool saveColor, uint8_t ldim)
#endif
{
  // As all functions have been converted to start, count...
  // rather than duplicating this in each function, do it once for each
  // setPixel() function
  /*
  if (n >= control_vars.pixel_count)
  {
    n -= control_vars.pixel_count;
  }*/

  if ( saveColor )
  {
    rgbBuffer[n] = rgb;
  }

  if ( ldim )
  {
    uint8_t dim = ldim;
#if defined (CONFIG_AEOLIAN_RGB_ORDER)
    outBuffer[n] = toRGBPixel ((int)rgb.r >> dim, (int)rgb.g >> dim, (int)rgb.b >> dim);
#else
    outBuffer[n] = toRGBPixel ((int)rgb.g >> dim, (int)rgb.r >> dim, (int)rgb.b >> dim);
#endif
  }
  else
  {
#if defined (CONFIG_AEOLIAN_RGB_ORDER)
    outBuffer[n] = toRGBPixel ((int)rgb.r, (int)rgb.g, (int)rgb.b);
#else
    outBuffer[n] = toRGBPixel ((int)rgb.g, (int)rgb.r, (int)rgb.b);
#endif
  }
}

// ******************************************************************
// ******************************************************************
void setPixelHSV (uint16_t n, cHSV hsv, bool saveColor, uint8_t ldim)
{
  setPixelRGB (n, hsv2rgb (hsv), saveColor, ldim);
}

// ******************************************************************
// Get a pixel
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB getPixelRGB (uint16_t n)
#else
IRAM_ATTR cRGB getPixelRGB (uint16_t n)
#endif
{
  return rgbBuffer[n];
}

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cHSV getPixelHSV (uint16_t n)
#else
IRAM_ATTR cHSV getPixelHSV (uint16_t n)
#endif
{
  return rgb2hsv (rgbBuffer[n]);
}

// ******************************************************************
// Fade the pixels
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
bool fadeToBlack (uint16_t i, uint8_t fadeValue)
#else
IRAM_ATTR bool fadeToBlack (uint16_t i, uint8_t fadeValue)
#endif
{
  bool faded = false;

  cHSV pixel = getPixelHSV (i);
  if ( pixel.v > 0 )
  {
    faded = true;

    if ( pixel.v > fadeValue )
    {
      pixel.v -= fadeValue;
    }
    else
    {
      pixel.s = 0;
      pixel.v = 0;
    }

    setPixelHSV (i, pixel, true, control_vars.dim);
  }

  return faded;
}

// ******************************************************************
// Restore the palette from the last run (with current dim settings)
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void restorePaletteAll (void)
#else
IRAM_ATTR void restorePaletteAll (void)
#endif
{
  for ( uint16_t n = 0; n < control_vars.pixel_count; n++ )
  {
    cRGB pixel = rgbBuffer[n];
    setPixelRGB (n, pixel, true, control_vars.dim);
  }
}

// ******************************************************************
// Partially restore the palette (with current dim settings)
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void restorePalette (uint16_t start, uint16_t count)
#else
IRAM_ATTR void restorePalette (uint16_t start, uint16_t count)
#endif
{
  uint16_t cur_pixel = start;
  uint16_t n = 0;

  while ( n < count )
  {
    cRGB pixel = rgbBuffer[cur_pixel];
    setPixelRGB (cur_pixel, pixel, true, control_vars.dim);

    // Check if we wrap around....
    if ( ++cur_pixel >= control_vars.pixel_count )
    {
      cur_pixel = 0;
    }

    // Next pixel...
    n++;
  }
}

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
bool rgbFadeAll (uint16_t start, uint16_t count, uint8_t fadeValue)
#else
IRAM_ATTR bool rgbFadeAll (uint16_t start, uint16_t count, uint8_t fadeValue)
#endif
{
  bool faded = false;

  uint16_t curPos = start;

  for ( int i = 0; i < count; i++ )
  {
    cRGB pixel = getPixelRGB (curPos);

    bool atZero = false;
    if ( pixel.r )
    {
      if ( pixel.r > fadeValue )
      {
        pixel.r -= fadeValue;
      }
      if ( pixel.r < MIN_DIM_LEVEL )
      {
        atZero = true;
      }
    }

    if ( pixel.g )
    {
      if ( pixel.g > fadeValue )
      {
        pixel.g -= fadeValue;
      }
      if ( pixel.g < MIN_DIM_LEVEL )
      {
        atZero = true;
      }
    }

    if ( pixel.b )
    {
      if ( pixel.b > fadeValue )
      {
        pixel.b -= fadeValue;
      }
      if ( pixel.b < MIN_DIM_LEVEL )
      {
        atZero = true;
      }
    }
    if ( atZero )
    {
      pixel.r = 0;
      pixel.g = 0;
      pixel.b = 0;
      faded = true;
    }

    // Set the pixel
    setPixelRGB (curPos, pixel, true, control_vars.dim);

    // Advance to the next pixel
    curPos = inc_curpos (curPos, start, count);
  }

  return faded;
}

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
bool hsvFadeAll (uint16_t start, uint16_t count, uint8_t fadeValue)
#else
IRAM_ATTR bool hsvFadeAll (uint16_t start, uint16_t count, uint8_t fadeValue)
#endif
{
  bool faded = false;
  cHSV pixel;

  uint16_t curPos = start;
  for ( int i = 0; i < count; i++ )
  {
    pixel = getPixelHSV (curPos);

    if ( pixel.v > 0 )
    {
      faded = true;

      if ( pixel.v > (fadeValue + 40) )
      {
        pixel.v -= fadeValue;
      }
      else
      {
        pixel.s = 0;
        pixel.v = 0;
      }

      setPixelHSV (curPos, pixel, true, control_vars.pixel_count);
    }

    curPos = inc_curpos (curPos, start, count);
  }

  return faded;
}

// ******************************************************************
// ******************************************************************
uint16_t next_pos (uint16_t pos, uint16_t start, uint16_t count, bool dir)
{
  int tmp_pos;
  uint16_t new_pos;

  if ( dir == DIR_RIGHT )
  {
    tmp_pos = pos - 1;
  }
  else
  {
    tmp_pos = pos + 1;
  }

  if ( tmp_pos < 0 )
  {
    new_pos = control_vars.pixel_count - 1;
  }
  else if ( tmp_pos < start )
  {
    new_pos = (start + count);
  }
  else if ( tmp_pos > (start + count) )
  {
    new_pos = start;
  }
  else
  {
    new_pos = tmp_pos;
  }

  return new_pos;
}

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
bool logicalShift (uint16_t start, uint16_t count, bool dir, bool rot)
#else
IRAM_ATTR bool logicalShift (uint16_t start, uint16_t count, bool dir, bool rot)
#endif
{
  cRGB saved_pixel;
  uint16_t oldPos = start;
  uint16_t curPos = 0;

  saved_pixel = getPixelRGB (start);

  for ( uint16_t i = start; i < (count - 1); i++ )
  {
    curPos = next_pos (oldPos, start, count, dir);
    setPixelRGB (oldPos, getPixelRGB (curPos), true, 0);
    oldPos = curPos;
  }

  if ( rot )
  {
    setPixelRGB (oldPos, saved_pixel, true, 0);
  }

  return false;
}

// ******************************************************************
/* Helper functions */
// Define the wheel function to interpolate between different hues.
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB wheel (unsigned int pos)
#else
IRAM_ATTR cRGB wheel (unsigned int pos)
#endif
{
  cRGB hue;

  if ( pos < 85 )
  {
    hue.r = pos * 3;
    hue.g = 255 - (pos * 3);
    hue.b = 0;
  }
  else if ( pos < 170 )
  {
    pos -= 85;
    hue.r = 255 - (pos * 3);
    hue.g = 0;
    hue.b = pos * 3;
  }
  else
  {
    pos -= 170;
    hue.r = 0;
    hue.g = pos * 3;
    hue.b = 255 - (pos * 3);
  }

  return hue;
}

#if defined (FAST_HSV)
// ******************************************************************
// *       RGB to HSV conversion courtesy of StackOverflow          *
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cHSV rgb2hsv(cRGB rgb)
#else
IRAM_ATTR cHSV rgb2hsv(cRGB rgb)
#endif
{
  cHSV hsv;
  unsigned char rgbMin, rgbMax;

  rgbMin = rgb.r < rgb.g?(rgb.r < rgb.b?rgb.r:rgb.b):(rgb.g < rgb.b?rgb.g:rgb.b);
  rgbMax = rgb.r > rgb.g?(rgb.r > rgb.b?rgb.r:rgb.b):(rgb.g > rgb.b?rgb.g:rgb.b);

  hsv.v = rgbMax;
  if ( hsv.v == 0 )
  {
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }

  hsv.s = 255 * long (rgbMax - rgbMin) / hsv.v;
  if ( hsv.s == 0 )
  {
    hsv.h = 0;
    return hsv;
  }

  if ( rgbMax == rgb.r )
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
  else if ( rgbMax == rgb.g )
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
  else
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

  return hsv;
}
// ******************************************************************
// *       HSV to RGB conversion courtesy of StackOverflow          *
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB hsv2rgb(cHSV hsv)
#else
IRAM_ATTR cRGB hsv2rgb(cHSV hsv)
#endif
{
  cRGB rgb;
  unsigned char region, remainder, p, q, t;

  if ( hsv.s == 0 )
  {
    rgb.r = hsv.v;
    rgb.g = hsv.v;
    rgb.b = hsv.v;
    return rgb;
  }

  region = hsv.h / 43;
  remainder = (hsv.h - (region * 43)) * 6;

  p = (hsv.v * (255 - hsv.s)) >> 8;
  q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
  t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

  switch ( region )
  {
    case 0:
      rgb.r = hsv.v; rgb.g = t; rgb.b = p;
      break;
    case 1:
      rgb.r = q; rgb.g = hsv.v; rgb.b = p;
      break;
    case 2:
      rgb.r = p; rgb.g = hsv.v; rgb.b = t;
      break;
    case 3:
      rgb.r = p; rgb.g = q; rgb.b = hsv.v;
      break;
    case 4:
      rgb.r = t; rgb.g = p; rgb.b = hsv.v;
      break;
    default:
      rgb.r = hsv.v; rgb.g = p; rgb.b = q;
      break;
  }

  return rgb;
}

#else //FAST_HSV
// ******************************************************************
// *       RGB to HSV conversion courtesy of StackOverflow          *
// ******************************************************************
// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cHSV rgb2hsv (cRGB rgb)
#else
IRAM_ATTR cHSV rgb2hsv (cRGB rgb)
#endif
{
  float min, max, delta;
  cHSV hsv;

  min = rgb.r < rgb.g?rgb.r:rgb.g;
  min = min < rgb.b?min:rgb.b;

  max = rgb.r > rgb.g?rgb.r:rgb.g;
  max = max > rgb.b?max:rgb.b;

  hsv.v = max;                                                       // v
  delta = max - min;
  if ( delta < 0.00001 )
  {
    hsv.s = 0;
    hsv.h = 0;                                                       // undefined, maybe nan?
  }
  else if ( max == 0.0 )                                             // if max is 0, then r = g = b = 0
  {                                                                  // s = 0, h is undefined
    hsv.s = 0.0;
    hsv.h = 0;                                                       // its now undefined
    return hsv;
  }
  else
  {
    hsv.s = (delta / max);                                         // s

    if ( rgb.r >= max )                                              // > is bogus, just keeps compilor happy
    {
      hsv.h = (rgb.g - rgb.b) / delta;                             // between yellow & magenta
    }
    else if ( rgb.g >= max )
    {
      hsv.h = 2.0 + (rgb.b - rgb.r) / delta;                       // between cyan & yellow
    }
    else
    {
      hsv.h = 4.0 + (rgb.r - rgb.g) / delta;                       // between magenta & cyan
    }

    hsv.h *= 60.0;                                                   // degrees

    if ( hsv.h < 0.0 )
      hsv.h += 360.0;
  }

  return hsv;
}
// ******************************************************************
// *       HSV to RGB conversion courtesy of StackOverflow          *
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB hsv2rgb (cHSV hsv)
#else
IRAM_ATTR cRGB hsv2rgb (cHSV hsv)
#endif
{
  float hh, p, q, t, ff;
  long i;
  cRGB rgb;

  if ( hsv.s <= 0.0 )
  {
    rgb.r = hsv.v;
    rgb.g = hsv.v;
    rgb.b = hsv.v;
  }
  else
  {
    hh = hsv.h;
    if ( hh >= 360.0 )
      hh = 0.0;
    hh /= 60.0;

    i = (long)hh;
    ff = hh - i;
    p = hsv.v * (1.0 - hsv.s);
    q = hsv.v * (1.0 - (hsv.s * ff));
    t = hsv.v * (1.0 - (hsv.s * (1.0 - ff)));

    switch ( i )
    {
      case 0:
        rgb.r = hsv.v;
        rgb.g = t;
        rgb.b = p;
        break;
      case 1:
        rgb.r = q;
        rgb.g = hsv.v;
        rgb.b = p;
        break;
      case 2:
        rgb.r = p;
        rgb.g = hsv.v;
        rgb.b = t;
        break;
      case 3:
        rgb.r = p;
        rgb.g = q;
        rgb.b = hsv.v;
        break;
      case 4:
        rgb.r = t;
        rgb.g = p;
        rgb.b = hsv.v;
        break;
      case 5:
      default:
        rgb.r = hsv.v;
        rgb.g = p;
        rgb.b = q;
        break;
    }
  }

  return rgb;
}
#endif // FAST_HSV

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB colorfade (cRGB pixel)
#else
IRAM_ATTR cRGB colorfade (cRGB pixel)
#endif
{
  cRGB fpix = pixel;

  if ( pixel.r > 0 && pixel.b == 0 )
  {
    fpix.r--;
    fpix.g++;
  }
  if ( pixel.g > 0 && pixel.r == 0 )
  {
    fpix.g--;
    fpix.b++;
  }
  if ( pixel.b > 0 && pixel.g == 0 )
  {
    fpix.r++;
    fpix.b--;
  }
  return fpix;
}

// ******************************************************************
// ******************************************************************
#define BLEND(A, B, C) ((A)-(C)*((A)-(B)))
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cHSV blendHSV (cHSV a, cHSV b, float c)
#else
IRAM_ATTR cHSV blendHSV (cHSV a, cHSV b, float c)
#endif
{
  cHSV tmp;

  tmp.h = BLEND (a.h, b.h, c);
  tmp.s = BLEND (a.s, b.s, c);
  tmp.v = BLEND (a.v, b.v, c);

  return tmp;
}

// ******************************************************************
// ******************************************************************
cRGB blendRGB (cRGB c1, cRGB c2)
{
  cRGB result;

  result.r = (int)MIN (c1.r + c2.r, 256);
  result.g = (int)MIN (c1.g + c2.g, 256);
  result.b = (int)MIN (c1.b + c2.b, 256);

  return result;
}

// ******************************************************************
// ******************************************************************
cRGB blendRGBorig (cRGB c1, cRGB c2)
{
  cRGB result;

  result.r = (int)(c1.r + c2.r) / 2.0;
  result.g = (int)(c1.g + c2.g) / 2.0;
  result.b = (int)(c1.b + c2.b) / 2.0;

  return result;
}

// ******************************************************************
// Utility:
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cHSV hsvFromPalette (const CRGBPalette16 pal, uint8_t index, uint8_t brightness)
#else
IRAM_ATTR cHSV hsvFromPalette (const CRGBPalette16 pal, uint8_t index, uint8_t brightness)
#endif
{
  uint8_t ci = index & 0xF;

  cHSV pixel = rgb2hsv ((cRGB)pal.entries[ci]);

  // Only set brightness if the colour is not black
  if ( pixel.v > 0 )
  {
    pixel.v = brightness;
  }

  return pixel;
}
// ******************************************************************
// Utility: ?
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
cRGB rgbFromPalette (const CRGBPalette16 pal, uint8_t index)
#else
IRAM_ATTR cRGB rgbFromPalette (const CRGBPalette16 pal, uint8_t index)
#endif
{
  uint8_t ci = index & 0xF;

  return (cRGB)pal.entries[ci];
}

// ******************************************************************
// Utility: Fill from palette
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void FillLEDsFromPaletteColors (uint16_t startIndex, uint16_t start, uint16_t count)
#else
IRAM_ATTR void FillLEDsFromPaletteColors (uint16_t startIndex, uint16_t start, uint16_t count)
#endif
{
  uint16_t curPos = start;

  F_LOGV(true, true, LC_GREY, "FillLEDsFromPaletteColors(%d, %d, %d ...", startIndex, start, count);

  for ( uint16_t i = 0; i < count; i++ )
  {
    //printf("i: %d, curPos: %d, startIndex: %d, start: %d, count: %d\n", i, curPos, startIndex, start, count);
    setPixelRGB(curPos, rgbFromPalette(curPalette[control_vars.thisPalette], ((curPos + startIndex) / control_vars.palette_stretch)), true, control_vars.dim);

    curPos = inc_curpos (curPos, start, count);
  }
}

// ******************************************************************
// Utility: Fill with RGB colour
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void rgbFillSolid (uint16_t start, uint16_t count, cRGB colour, bool save, uint8_t ldim)
#else
IRAM_ATTR void rgbFillSolid (uint16_t start, uint16_t count, cRGB colour, bool save, uint8_t ldim)
#endif
{
  uint16_t curPos = start;

  for ( uint16_t i = 0; i < count; i++ )
  {
    setPixelRGB (curPos, colour, save, ldim);

    curPos = inc_curpos (curPos, start, count);
  }
}

// ******************************************************************
// Utility: Fill with HSV colour
// ******************************************************************
#if defined (CONFIG_APPTRACE_SV_ENABLE)
void hsvFillSolid (uint16_t start, uint16_t count, cHSV colour, bool save)
#else
IRAM_ATTR void hsvFillSolid (uint16_t start, uint16_t count, cHSV colour, bool save)
#endif
{
  uint16_t curPos = start;

  for ( uint16_t i = 0; i < count; i++ )
  {
    setPixelHSV (curPos, colour, save, control_vars.dim);

    curPos = inc_curpos (curPos, start, count);
  }
}

// ******************************************************************
// Utility: Convert pause reason to string
// ******************************************************************
const char *reason2str (pause_event_t reason)
{
  char *str = NULL;

  switch ( reason )
  {
    case PAUSE_NOTPROVIDED:
      str = (char *)"not provided";
      break;
    case PAUSE_DAYTIME:
      str = (char *)"daylight hours";
      break;
    case PAUSE_BOOT:
      str = (char *)"boot init";
      break;
    case PAUSE_SCHEDULE:
      str = (char *)"scheduled task";
      break;
    case PAUSE_EVENT:
      str = (char *)"event";
      break;
    case PAUSE_MASTER_REQ:
      str = (char *)"master request";
      break;
    case PAUSE_USER_REQ:
      str = (char *)"user request";
      break;
    case PAUSE_UPLOADING:
      str = (char *)"uploading";
      break;
    default:
      str = (char *)"unknown";
      break;
  }

  return str;
}

// ******************************************************************
// Debug: Print pause flags
// ******************************************************************
#if defined (CONFIG_DEBUG_PAUSE)
#define BUF_SIZE         256
const char *pre_unpause   = " Pre Unpause";
const char *post_unpause  = "Post Unpause";
const char *pre_pause     = " Pre Pause";
const char *post_pause    = "Post Pause";
void printPausedFlags (const char *prefix, uint16_t bitflags)
{
  char comma = 0x00;
  char tmpbuf[BUF_SIZE + 1] = {};
  char binstring[64];
  int  bufptr = 0;
  uint16_t j, bit = 1;

  for ( j = 0; j < 8; j++, bit <<= 1 )
  {
    if ( BTST (bitflags, bit) )
    {
      bufptr += snprintf(&tmpbuf[bufptr], BUF_SIZE - bufptr, "%s%s", (comma?", ":""), reason2str((pause_event_t)bit));
      comma = 0x01;
    }
  }

  char *binPtr = binary (binstring, 64, bitflags);
  if ( bufptr )
  {
    F_LOGI(true, true, LC_MAGENTA, "%s: %s => %s", prefix, binPtr, tmpbuf);
  }
  else
  {
    F_LOGI(true, true, LC_MAGENTA, "%s: %s => (no flags set)", prefix, binPtr);
  }
}
#endif

// ******************************************************************
// * Update and sync our settings to the network (if master)        *
// ******************************************************************
void updateSync(void)
{
  static uint16_t change_delay = 0;

  // Do any changes to the pattern, that will be synced to other clients,
  // immediately prior to syncing across the network. This reduces the number
  // of packets we need and creates a more seamless display.
  // ------------------------------------------------------
  if ( !BTST(control_vars.bitflags, DISP_BF_SLAVE) )
  {
    if ( !change_delay )
    {
      bool set_delay = false;
      // esp_ranom()
      // This function automatically busy-waits to ensure enough external entropy has been introduced
      // into the hardware RNG state, before returning a new random number.
      // This delay is very short (always less than 100 CPU cycles).
      // Returns: Random value between 0 and UINT32_MAX
      long temp_rand = prng();

      // Pick a random colour from the current palette
      // ------------------------------------------------------
      if ( ((temp_rand >> 16) & 0xF) < 2 )
      {
        set_thisCol();
        set_delay = true;
        //printf("set_thisCol\n");
      }

      if ( ((temp_rand >> 24) & 0xF) < 2 )
      {
        set_thatCol();
        set_delay = true;
        //printf("set_thatCol\n");
      }

      // Change the palette every now and then
      // ------------------------------------------------------
      if ( (temp_rand & 0x1313) == 0x1313 )
      {
        control_vars.thisPalette = (prng() % paletteSize);
        set_delay = true;
        //log(ESP_LOG_VERBOSE, true, true, LC_WHITE, "set thisPalette: %d", control_vars.thisPalette);
      }

      if ( (temp_rand & 0x1234) == 0x1234 )
      {
        control_vars.thatPalette = (prng() % paletteSize);
        set_delay = true;
        //log(ESP_LOG_VERBOSE, true, true, LC_WHITE, "set thatPalette: %d", control_vars.thatPalette);
      }

      if ( (temp_rand & 0x1202020) == 0x1202020 )
      {
        BCHG(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
        set_delay = true;
        //log(ESP_LOG_VERBOSE, true, true, LC_WHITE, "BCHG(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) => %d", BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT));
      }

      if ( (temp_rand & 0x1011011) == 0x1011011 )
      {
        BCHG(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT);
        set_delay = true;
        //log(ESP_LOG_VERBOSE, true, true, LC_WHITE, "BCHG(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT) => %d", BTST(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT));
      }

      if ( ((temp_rand >> 8) & 0xF) < 2 )
      {
        control_vars.palIndex = prng() & 0xF;
        set_delay = true;
      }

      // Do we need to set a delay until we can make another change to the pattern?
      // We are called once a second, so the minimum delay is 1 second.
      if ( set_delay )
      {
        change_delay = 5;
      }
    }
    else
    {
      change_delay--;
    }
  }

  if ( MQTT_Client_Cfg.Master == true )
  {
    mqttControl(dev_LEDs, JSON_SYNC_STR, control_vars.slaves.paused, control_vars.slaves.pauseReason,
                                        BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT), BTST(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT),
                                        control_vars.thisPalette, control_vars.thatPalette,
                                        control_vars.cur_themeID, control_vars.cur_pattern,
                                        control_vars.dim, control_vars.delay_us,
                                        control_vars.this_col.r, control_vars.this_col.g, control_vars.this_col.b,
                                        control_vars.that_col.r, control_vars.that_col.g, control_vars.that_col.b,
                                        control_vars.static_col.r, control_vars.static_col.g, control_vars.static_col.b);
  }
}

// ******************************************************************
// * Resume lights for this reason                                  *
// ******************************************************************
void lightsUnpause (uint16_t reason, bool clearOnly)
{
#if defined (CONFIG_DEBUG_PAUSE)
  char tmpbuf[256];
  snprintf(tmpbuf, 255, "U: reason: %d, clearOnly: %d, schedule: %d", reason, clearOnly, control_vars.schedule);
  printPausedFlags(tmpbuf, control_vars.pauseFlags);
#endif

  // Clear the bit flag.
  // -----------------------------------------------------------------
  BCLR(control_vars.pauseFlags, reason);

  // Only process the core functionality if we are initialised.
  // -----------------------------------------------------------------
  if ( BTST(control_vars.bitflags, DISP_BF_INITIALISED) )
  {
    // We switch the lights on if:
    // 1. "clearOnly" is false and "pauseFlags" has no bits set.
    // 2. "PAUSE_UPLOADING" bitflag is not set and control_vars.schedule == SCHED_ON.
    // -----------------------------------------------------------------
    if ( ((clearOnly == false) && control_vars.pauseFlags == 0) || (!BTST(reason, PAUSE_UPLOADING) && !control_vars.schedule) )
    {
      // Only process the switch code if we have to
      if ( BTST(control_vars.bitflags, DISP_BF_PAUSED) )
      {
        // Switch the lights on
        BCLR(control_vars.bitflags, DISP_BF_PAUSED);

        // Update our LEDs status
        mqttPublish(dev_LEDs, "Unpaused: %s", reason2str((pause_event_t)reason));

        // Command our minions, if applicable
        if ( MQTT_Client_Cfg.Master == true && reason != PAUSE_UPLOADING )
        {
          control_vars.slaves.paused = false;
          control_vars.slaves.pauseReason = PAUSE_MASTER_REQ;

          // Immediate control
          //mqttControl(dev_LEDs, JSON_SWITCHON_STR, CMD_LED_PWR, "on", control_vars.slaves.pauseReason, control_vars.delay_us, control_vars.cur_themeID, control_vars.cur_pattern);
        }
      }
    }
  }

  // Debug
#if defined (CONFIG_DEBUG_PAUSE)
  printPausedFlags (post_unpause, control_vars.pauseFlags);
#endif
}

// ******************************************************************
// * Pause lights for reason                                        *
// ******************************************************************
void lightsPause (uint16_t reason)
{
#if defined (CONFIG_DEBUG_PAUSE)
  char tmpbuf[256];
  snprintf(tmpbuf, 255, "P: reason: %d, schedule: %d", reason, control_vars.schedule);
  printPausedFlags(tmpbuf, control_vars.pauseFlags);
#endif

  // Set the bitflag
  BSET(control_vars.pauseFlags, reason);

  // We always switch off if:
  // 1. We are uploading a new firmware (so as to prevent flickering lights when writing to flash)
  // 2. control_vars.schedule != SCHED_ON and pauseFlags > 0
  if ( BTST(control_vars.pauseFlags, PAUSE_UPLOADING) || (control_vars.pauseFlags && control_vars.schedule) )
  {
    // Only process the main switch code if we have to.
    if ( !BTST(control_vars.bitflags, DISP_BF_PAUSED) )
    {
      // Turn out the LEDs
      BSET(control_vars.bitflags, DISP_BF_PAUSED);

      // Update our LEDs status
      mqttPublish(dev_LEDs, "Paused: %s", reason2str((pause_event_t)reason));
    }

    // Publish to slaves, if applicable (we are a master and not updating our firmware)
    if ( MQTT_Client_Cfg.Master == true && reason != PAUSE_UPLOADING && !control_vars.slaves.paused )
    {
      // Save these for updateSync()
      control_vars.slaves.paused = true;
      control_vars.slaves.pauseReason = PAUSE_MASTER_REQ;

      // Immediate control
      //mqttControl(dev_LEDs, JSON_SWITCHOFF_STR, CMD_LED_PWR, "off", control_vars.slaves.pauseReason);
    }
  }

  // Debug
#if defined (CONFIG_DEBUG_PAUSE)
  printPausedFlags(post_pause, control_vars.pauseFlags);
#endif
}

// ******************************************************************
// * Are we paused for this specific reason?                        *
// ******************************************************************
bool lightsPausedReason(uint16_t reason)
{
  return BTST(control_vars.pauseFlags, reason);
}

// ******************************************************************
// * Are we actually paused?                                        *
// ******************************************************************
bool lightsPaused (void)
{
  return BTST(control_vars.bitflags, DISP_BF_PAUSED);
}

// ******************************************************************
// * Reboot, fuck yeh!                                              *
// ******************************************************************
void initReboot ()
{
  BSET(control_vars.bitflags, DISP_BF_REBOOT);
}

// ******************************************************************
// * Debug: Testing rgb2hsv works properly                          *
// ******************************************************************
void test_rgb2hsv (cRGB colour)
{
  cHSV pixela = rgb2hsv(colour);
  cRGB pixelb = hsv2rgb(pixela);

  F_LOGI(true, true, LC_BRIGHT_WHITE, "------------------------------------------------------------------");
  F_LOGI(true, true, LC_BRIGHT_GREEN, "  Initial RGB: 0x%02X, 0x%02X, 0x%02X", colour.r, colour.g, colour.b);
  F_LOGI(true, true, LC_BRIGHT_GREEN, "Converted HSV: %d, %d, %d", pixela.h, pixela.s, pixela.v);
  F_LOGI(true, true, LC_BRIGHT_GREEN, "Converted RGB: 0x%02X, 0x%02X, 0x%02X", pixelb.r, pixelb.g, pixelb.b);

  return;
  uint16_t scale_fixed = 240;
  for ( uint16_t p = 15; p < 70; p++ )
  {
    uint8_t x = 255;

    if ( pixelb.r )
      x = (pixelb.r = (((uint16_t)pixelb.r) * scale_fixed) >> 8);

    if ( pixelb.g )
      x = MIN (x, (pixelb.g = (((uint16_t)pixelb.g) * scale_fixed) >> 8));

    if ( pixelb.b )
      x = MIN (x, (pixelb.b = (((uint16_t)pixelb.b) * scale_fixed) >> 8));

    //if (MAX(pixelb.r, MAX(pixelb.g, pixelb.b)) < 0x0C || x)
    if ( x < 6 )
    {
      F_LOGI(true, true, LC_BRIGHT_GREEN, "    Faded RGB: 0x%02X, 0x%02X, 0x%02X   %d *", pixelb.r, pixelb.g, pixelb.b, x);
      setPixelRGB (p, CRGBBlack, true, control_vars.dim);
    }
    else
    {
      F_LOGI(true, true, LC_BRIGHT_GREEN, "    Faded RGB: 0x%02X, 0x%02X, 0x%02X   %d", pixelb.r, pixelb.g, pixelb.b, x);
      setPixelRGB (p, pixelb, true, control_vars.dim);
    }
  }
}

// ******************************************************************
// * Utility: Set this/that colours                                 *
// ******************************************************************
void set_thisCol (void)
{
  control_vars.this_col = rgbFromPalette(curPalette[control_vars.thisPalette], (prng() % 16));
}
void set_thatCol (void)
{
  control_vars.that_col = rgbFromPalette(curPalette[control_vars.thatPalette], (prng() % 16));
}

// ******************************************************************
// Utility: Search for a pattern by name.
// Optionally, change the display to that pattern.
// ******************************************************************
int16_t getPatternByName (const char *name, bool sel)
{
  int16_t found = -1;

  for ( uint16_t cp = 0; cp <= control_vars.num_patterns; cp++ )
  {
    if ( strcmp (patterns[cp].name, name) == 0 )
    {
      found = cp;
      if ( sel )
      {
        set_pattern (cp);
      }
      break;
    }
  }

  return found;
}

// ******************************************************************
// Utility: Set brightness level
// ******************************************************************
bool set_brightness(uint8_t level)
{
  bool success = false;

  if ( level <= DIM_TOTAL )
  {
    control_vars.dim = level;
    success = true;
  }

  return success;
}

// ******************************************************************
// Utility: Set loop duration
// ******************************************************************
bool set_loop_delay(uint32_t delay_us)
{
  bool success = false;

  if ( delay_us >= 14000 )
  {
    control_vars.delay_us = delay_us;
    success = true;
  }

  return success;
}

// ******************************************************************
// Utility: Get theme by ID
// ******************************************************************
uint8_t get_theme_idx_by_id (uint8_t themeID)
{
  for ( uint8_t ti = 0; ti < control_vars.num_themes; ti++ )
  {
    if ( themes[ti].themeIdentifier == themeID )
    {
      return ti;
    }
  }

  return 0xff;
}

// ******************************************************************
// Utility: Get theme by name
// ******************************************************************
const char *get_theme_name (uint8_t themeName)
{
  uint8_t index = get_theme_idx_by_id (themeName);

  if ( index != 0xff )
  {
    return themes[index].name;
  }

  return NULL;
}

// ******************************************************************
// Set theme
// ******************************************************************
esp_err_t set_theme(uint8_t themeID, uint8_t dim)
{
  esp_err_t err = ESP_FAIL;

  uint8_t index = get_theme_idx_by_id(themeID);

  F_LOGW(true, true, LC_YELLOW, "cur_themeID: %d, cur_themeidx: %d", themeID, index);

  // Process the request if a valid themeID was found and not already set
  if ( (index != 0xFF) && index != control_vars.cur_themeIdx )
  {
    err = ESP_OK;

    control_vars.cur_themeIdx = index;
    control_vars.cur_themeID = themeID;

    if ( dim != 0xFF )
    {
      control_vars.theme_dim = dim;
      set_brightness(control_vars.theme_dim);
    }

    curPalette = themes[control_vars.cur_themeIdx].list;
    paletteSize = themes[control_vars.cur_themeIdx].count;

    // Set 'this_col' and 'that_col'
    set_thisCol();
    set_thatCol();

    // Random daily colour
    control_vars.static_col = rgbFromPalette (curPalette[control_vars.thisPalette], prng() & 0xF);

    F_LOGI(true, true, LC_BRIGHT_GREEN, "Current theme: %s (%d colour palettes)", themes[control_vars.cur_themeIdx].name, paletteSize);

    if ( !BTST(patterns[control_vars.cur_pattern].mask, themes[control_vars.cur_themeIdx].patternMask) )
    {
      switch_pattern();
    }
  }

  return err;
}

// ******************************************************************
// Set pattern
// ******************************************************************
bool set_pattern (uint16_t pattern)
{
  bool valid = false;

  if ( pattern <= control_vars.num_patterns && control_vars.cur_pattern != pattern )
  {
    valid = true;
    control_vars.cur_pattern = pattern;

    // Notify of our change
    mqttPublish (dev_Pattern, patterns[pattern].name);

    // Publish to slaves, if applicable
    mqttControl(dev_Pattern, patterns[pattern].name);
  }

  return valid;
}

// ******************************************************************
// Choose a random pattern and switch to it
// ******************************************************************
bool switch_pattern()
{
  bool valid = false;
  uint16_t cp;
  uint16_t pn = 0;

#if defined (FORCE_PATTERN)
  int16_t fp = getPatternByName (FORCE_PATTERN, true);
  if ( fp >= 0 )
  {
    return set_pattern (fp);
  }
#endif

  for ( cp = 0; cp <= control_vars.num_patterns; cp++ )
  {
    if ( (patterns[cp].enabled == true) && (BTST (patterns[cp].mask, themes[control_vars.cur_themeIdx].patternMask)) )
    {
      pn++;
    }
  }

  // Logging
  F_LOGI(true, true, LC_BRIGHT_BLUE, "%d patterns out of %d available for current theme: %s", pn, control_vars.num_patterns, themes[control_vars.cur_themeIdx].name);

  // If zero, we didn't find any valid patterns, so skip
  if ( pn > 0 )
  {
  // Select one of the patterns
    uint16_t sp = (prng () % pn) + 1;

    for ( cp = 0, pn = 0; cp <= control_vars.num_patterns; cp++ )
    {
      if ( (patterns[cp].enabled == true) && (BTST (patterns[cp].mask, themes[control_vars.cur_themeIdx].patternMask)) )
      {
        if ( ++pn == sp )
        {
          break;
        }
      }
    }

    // Debug log
    F_LOGI(true, true, LC_BRIGHT_BLUE, "Selected pattern: '%s'", patterns[cp].name);

    // cp should be the pattern we picked randomly, above.
    valid = set_pattern (cp);
  }

  return valid;
}

// ******************************************************************
// Called once a day and on start up to change some things
// ******************************************************************
void lights_dailies (void)
{
  if ( control_vars.pattern_cycle == PATTERN_DAILY )
  {
    switch_pattern();
  }
}

// ******************************************************************
// Called every 60 seconds
// ******************************************************************
void lights_sixty (void)
{
  if ( control_vars.pattern_cycle == PATTERN_RANDOM )
  {
    switch_pattern();
  }

  // Random daily colour
  control_vars.static_col = rgbFromPalette (curPalette[control_vars.thisPalette], prng () & 0xF);
}

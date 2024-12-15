#ifndef __UTILS_H__
#define __UTILS_H__

#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <esp_log.h>

#ifndef _BASE64_H_
#define _BASE64_H_

#include <vector>
#include <string>
typedef unsigned char BYTE;

#endif

#undef MIN
#undef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#define MIN(a,b) (((a)<(b)) ? (a) : (b))


// Bit operations
// --------------------------------------------------------------------
#define BTST(a,b) (((a) & (b)) > 0)
#define BSET(a,b) ((a) |= (b))
#define BCLR(a,b) ((a) &= ~(b))
#define BCHG(a,b) ((a) = (a) ^ (b))

#define CONST_PI    3.14159
#define CONST_TAU   6.28318

#define F_LOGE(show_timestamp, linefeed, colour, format, ...) \
      if (CONFIG_APP_LOG_LEVEL >= ESP_LOG_ERROR)   { log(ESP_LOG_ERROR, show_timestamp, linefeed, colour, format, ##__VA_ARGS__); }
#define F_LOGW(show_timestamp, linefeed, colour, format, ...) \
      if (CONFIG_APP_LOG_LEVEL >= ESP_LOG_WARN)    { log(ESP_LOG_WARN, show_timestamp, linefeed, colour, format, ##__VA_ARGS__); }
#define F_LOGI(show_timestamp, linefeed, colour, format, ...) \
      if (CONFIG_APP_LOG_LEVEL >= ESP_LOG_INFO)    { log(ESP_LOG_INFO, show_timestamp, linefeed, colour, format, ##__VA_ARGS__); }
#define F_LOGD(show_timestamp, linefeed, colour, format, ...) \
      if (CONFIG_APP_LOG_LEVEL >= ESP_LOG_DEBUG)   { log(ESP_LOG_DEBUG, show_timestamp, linefeed, colour, format, ##__VA_ARGS__); }
#define F_LOGV(show_timestamp, linefeed, colour, format, ...) \
      if (CONFIG_APP_LOG_LEVEL >= ESP_LOG_VERBOSE) { log(ESP_LOG_VERBOSE, show_timestamp, linefeed, colour, format, ##__VA_ARGS__); }

#define MAX_URI_PARTS               8

#define BLACK                       "\033[0;30m"
#define RED                         "\033[0;31m"
#define GREEN                       "\033[0;32m"
#define YELLOW                      "\033[0;33m"
#define BLUE                        "\033[0;34m"
#define MAGENTA                     "\033[0;35m"
#define CYAN                        "\033[0;36m"
#define WHITE                       "\033[0;37m"
#define GREY                        "\033[0;90m"
#define BRIGHT_RED                  "\033[0;91m"
#define BRIGHT_GREEN                "\033[0;92m"
#define BRIGHT_YELLOW               "\033[0;93m"
#define BRIGHT_BLUE                 "\033[0;94m"
#define BRIGHT_MAGENTA              "\033[0;95m"
#define BRIGHT_CYAN                 "\033[0;96m"
#define BRIGHT_WHITE                "\033[0;97m"

#define ANSI_COLOR_RESET            "\033[0;0m"

#define START_COLOR                 CYAN_COLOR
#define STOP_COLOR                  WHITE_COLOR

#if defined (APP_DEBUG_LEVEL)
#define APP_LOG_LEVEL               APP_DEBUG_LEVEL
#else
#define APP_LOG_LEVEL               1
#endif

#define BINARY_PATTERN_INT16        BINARY_PATTERN_INT8 BINARY_PATTERN_INT8
#define BYTE_TO_BINARY_INT16(i)     BYTE_TO_BINARY_INT8((i) >> 8), BYTE_TO_BINARY_INT8(i)

#define BINARY_PATTERN_INT32        BINARY_PATTERN_INT16 BINARY_PATTERN_INT16
#define BYTE_TO_BINARY_INT32(i)     BYTE_TO_BINARY_INT16((i) >> 16), BYTE_TO_BINARY_INT16(i)

#define BINARY_PATTERN_INT64        BINARY_PATTERN_INT32 BINARY_PATTERN_INT32
#define BYTE_TO_BINARY_INT64(i)     BYTE_TO_BINARY_INT32((i) >> 32), BYTE_TO_BINARY_INT32(i)

#define get_tmr(a)                  (xTaskGetTickCount() + (a))
#define check_tmr(a)                (xTaskGetTickCount() >= (a) ? true : false)
#define delay_ms(a)                 (vTaskDelay((a) / portTICK_PERIOD_MS))

#define JSON_RESPONSE_AS_INTVAL     "{\"%s\": %d}"
#define JSON_RESPONSE_AS_STRVAL     "{\"%s\": %s}"
#define JSON_RESPONSE_AS_STRING     "{\"%s\": \"%s\"}"

union xs_u {
uint32_t x[10];
uint64_t y[5];
};
struct xor_state
{
  xs_u u;
  uint32_t counter;
};
extern xor_state xs;

typedef enum log_colour
{
  LC_BLACK,
  LC_RED,
  LC_GREEN,
  LC_YELLOW,
  LC_BLUE,
  LC_MAGENTA,
  LC_CYAN,
  LC_WHITE,
  LC_GREY,
  LC_BRIGHT_RED,
  LC_BRIGHT_GREEN,
  LC_BRIGHT_YELLOW,
  LC_BRIGHT_BLUE,
  LC_BRIGHT_MAGENTA,
  LC_BRIGHT_CYAN,
  LC_BRIGHT_WHITE,
} log_colour_t;

#define RANDOM8_MAX(lim)            (random8() % (1 + lim))
#define RANDOM8_MIN_MAX(min,lim)    ((random8() % (1 + (lim - min))) + min)
#define RANDOM16_MAX(lim)           (random16() % (1 + lim))
#define RANDOM16_MIN_MAX(min,lim)   ((random16() % (1 + (lim - min))) + min)

/// @param theta input angle from 0-255
/// @returns 0 - 255 (starting at 127 for 0)
inline uint8_t sin8(uint8_t theta)
{
  //return (uint8_t)(127 * sin((theta * (3.14159 * 2)) / 255) + 127);
  return (uint8_t)(127.5 * sin(theta / (255 / CONST_TAU)) + 128);
}
/// @param theta input angle from 0-255
/// @returns 0 - 255 (starting at 254 for 0)
inline uint8_t cos8(uint8_t theta)
{
  //return (uint8_t)(127 * cos((theta * (3.14159 * 2)) / 255) + 127);
  return (uint8_t)(127.5 * cos(theta / (255 / CONST_TAU)) + 128);
}

///     float s = sin(x) * 32767.0;
///
/// @param theta input angle from 0-65535
/// @returns sin of theta, value between -32767 to 32767.
inline int16_t sin16(uint16_t theta)
{
  return (int16_t)(32767 * sin(theta / (65535 / CONST_TAU)));  // sin((theta * 3.14159 * 2) / 65535)
}
///     float s = cos(x) * 32767.0;
///
/// @param theta input angle from 0-65535
/// @returns sin of theta, value between -32767 to 32767.
inline int16_t cos16(uint16_t theta)
{
  return (int16_t)(32767 * cos(theta / (65535 / CONST_TAU)));  // sin((theta * 3.14159 * 2) / 65535)
}

// **************************************************
// ** Special case, ease of use functions
// **************************************************
/// @param theta input angle from 0-255
/// @returns 0 - 255 (starting at 0 for 0)
inline uint8_t zsin8(uint8_t theta)
{
  return (uint8_t)sin8(theta-64);
}
/// @param theta input angle from 0-255
/// @returns 0 - 255 (starting at 254 for 0)
inline uint8_t psin8(uint8_t theta)
{
  return (uint8_t)sin8(theta+64);
}


// **************************************************
// ** Modified lib8 functions
// **************************************************
uint_fast8_t  random8(void);
int_fast16_t  random16(void);


// **************************************************
// ** Other functions
// **************************************************
uint32_t     clock_ms(void);
void         prng_seed(void);
uint32_t     prng(xor_state *state=&xs);
void         log(const esp_log_level_t logLevel, const bool ts, const bool cr, const log_colour_t logCol, const char *format, ...);
uint32_t     get_log_messages(char **bufptr, uint32_t *logstart);
void        *copybytes(void *dst, const void *src, int len);
void         hexDump(const char* desc, const void* addr, uint16_t len, uint16_t perLine);
const char  *get_filename_ext (const char *filename);
uint32_t     hex2int(char *hex);
int          str_cmp(const char *arg1, const char *arg2);
int          shrt_cmp(const char *arg1, const char *arg2);
uint16_t     url_encode(char *dstBuffer, const char *str, uint16_t buflen);
uint16_t     url_decode(char *dstBuffer, const char *strURL, uint16_t buflen);
char *binary (char *buf, uint16_t bufSize, unsigned int val);
std::string  b64encode (const unsigned char *src, size_t len);
std::string  b64decode (const void *data, const size_t len);

#endif

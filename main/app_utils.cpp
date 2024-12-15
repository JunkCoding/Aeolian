

// Essential
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "esp_random.h"

#include "app_utils.h"
#include "app_yuarel.h"


// Serial
#include "driver/uart.h"

/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/
IRAM_ATTR uint32_t clock_ms()
{
  return ( uint32_t )(clock() * 1000 / CLOCKS_PER_SEC);
}

// Serial ports
extern uint16_t radSerial;

const char *colour[] =
{
  BLACK,
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGENTA,
  CYAN,
  WHITE,
  GREY,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_YELLOW,
  BRIGHT_BLUE,
  BRIGHT_MAGENTA,
  BRIGHT_CYAN,
  BRIGHT_WHITE
};

const char *err_prefix[] = {
  BLACK"",
  RED"E",
  YELLOW"W",
  GREEN"I",
  MAGENTA"D",
  GREY"V"
};


/*
* Base64 encoding/decoding (RFC1341)
* Copyright (c) 2005-2011, Jouni Malinen <[email protected]>
*
* This software may be distributed under the terms of the BSD license.
* See README for more details.
*/

// 2016-12-12 - Gaspard Petit : Slightly modified to return a std::string 
// instead of a buffer allocated with malloc.

#include <string>

static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool is_base64 (BYTE c)
{
  return (isalnum (c) || (c == '+') || (c == '/'));
}

// **************************************************************************************************
// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
// **************************************************************************************************
/**
* base64_encode - Base64 encode
* @src: Data to be encoded
* @len: Length of the data to be encoded
* @out_len: Pointer to output length variable, or %NULL if not used
* Returns: Allocated buffer of out_len bytes of encoded data,
* or empty string on failure
*/
std::string b64encode (const unsigned char *src, size_t len)
{
  unsigned char *out, *pos;
  const unsigned char *end, *in;

  size_t olen;

  olen = 4 * ((len + 2) / 3); /* 3-byte blocks to 4-byte */

  if ( olen < len )
    return std::string (); /* integer overflow */

  std::string outStr;
  outStr.resize (olen);
  out = (unsigned char *)&outStr[0];

  end = src + len;
  in = src;
  pos = out;
  while ( end - in >= 3 )
  {
    *pos++ = base64_table[in[0] >> 2];
    *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
    *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
    *pos++ = base64_table[in[2] & 0x3f];
    in += 3;
  }

  if ( end - in )
  {
    *pos++ = base64_table[in[0] >> 2];
    if ( end - in == 1 )
    {
      *pos++ = base64_table[(in[0] & 0x03) << 4];
      *pos++ = '=';
    }
    else
    {
      *pos++ = base64_table[((in[0] & 0x03) << 4) |
        (in[1] >> 4)];
      *pos++ = base64_table[(in[1] & 0x0f) << 2];
    }
    *pos++ = '=';
  }

  return outStr;
}
static const int B64index[256] = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
  0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

std::string b64decode (const void *data, const size_t len)
{
  unsigned char *p = (unsigned char *)data;
  int pad = len > 0 && (len % 4 || p[len - 1] == '=');
  const size_t L = ((len + 3) / 4 - pad) * 4;
  std::string str (L / 4 * 3 + pad, '\0');

  for ( size_t i = 0, j = 0; i < L; i += 4 )
  {
    int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
    str[j++] = n >> 16;
    str[j++] = n >> 8 & 0xFF;
    str[j++] = n & 0xFF;
  }
  if ( pad )
  {
    int n = B64index[p[L]] << 18 | B64index[p[L + 1]] << 12;
    str[str.size () - 1] = n >> 16;

    if ( len > L + 2 && p[L + 2] != '=' )
    {
      n |= B64index[p[L + 2]] << 6;
      str.push_back (n >> 8 & 0xFF);
    }
  }
  return str;
}
// **************************************************************************************************

// **************************************************************************************************
// * Use memcpy and return a ptr to the next character after the last byte copied.
// **************************************************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void *copybytes(void *dst, const void *src, int len)
#else
void *copybytes(void *dst, const void *src, int len)
#endif
{
  // Thanks to Stack Overflow for this one
  // https://stackoverflow.com/questions/26755638/warning-pointer-of-type-void-used-in-arithmetic
  return (( void * )((( uint8_t * )memcpy(dst, src, len)) + len));
}

// **************************************************************************************************
// * Log messages
// **************************************************************************************************
typedef struct
{
  uint32_t      line_number;
  uint16_t      mesg_len;
  char         *log_mesg;
  void         *next_logmesg;
  void         *prev_logmesg;
} backlog_struct_t;
#define MAX_LOG_LINES   CONFIG_APP_LOG_LINES
#define LOG_BUFSIZE     255

// logging history (for web interface, etc.)
static backlog_struct_t *latestlog = NULL;        // The newest log entry in the linked list.
static backlog_struct_t *oldestlog = NULL;        // The oldest log in the linked list.
static uint32_t log_line           = 0;           // Count of log lines processed / when to start trimming log entries.

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void log(const esp_log_level_t logLevel, const bool ts, const bool cr, const log_colour_t logCol, const char *format, ...)
#else
void log(const esp_log_level_t logLevel, const bool ts, const bool cr, const log_colour_t logCol, const char *format, ...)
#endif
{
  char tmpbuf[LOG_BUFSIZE + 1] = {};
  int len = 0;

  // Current time, including microseconds
  struct timeval tv;
  struct tm *dt;

  gettimeofday(&tv, NULL);
  dt = localtime (&tv.tv_sec);

  va_list args;
  va_start(args, format);
  if ( ts )
  {
    len += sprintf(&tmpbuf[len], "%s %s(%02d:%02d:%02d.%03d) ", err_prefix[logLevel], colour[LC_BRIGHT_CYAN], dt->tm_hour, dt->tm_min, dt->tm_sec, ( int )tv.tv_usec / 1000);
  }
  len += sprintf(&tmpbuf[len], "%s", colour[logCol]);
  len += vsnprintf(&tmpbuf[len], LOG_BUFSIZE, format, args);
  if ( cr )
  {
    len += sprintf(&tmpbuf[len], "%s\r\n", ANSI_COLOR_RESET);
  }
  else
  {
    len += sprintf(&tmpbuf[len], "%s", ANSI_COLOR_RESET);
  }
  va_end(args);

  // Log to console...
  // ----------------------------
  printf(tmpbuf);

  // Publish to MQTT
  // ----------------------------
  //mqtt.publish(mqtt_pub, tmpbuf);

  // ------------------------------------------------------------------------------------------
  // - This section is for an internal RAM based 'logfile', more specifically, a linked list. -
  // ------------------------------------------------------------------------------------------
  // ToDo: add a mutex to access the linked list.
  // Create a new log entry
  // ----------------------------
  backlog_struct_t *log_entry = (backlog_struct_t *)pvPortMalloc(sizeof(backlog_struct_t));
  log_entry->line_number      = log_line;
  log_entry->log_mesg         = (char *)pvPortMalloc(len);
  log_entry->mesg_len         = len;
  log_entry->next_logmesg     = NULL;         // Newer list entry
  log_entry->prev_logmesg     = NULL;         // Older list entry
  memcpy(log_entry->log_mesg, tmpbuf, len);

  // Check both, so if we have integer rollover, we don't screw up the list
  if ( (log_line++ == 0) && (NULL == oldestlog) )
  {
    oldestlog = log_entry;
  }
  // Else, check if we need to start trimming the linked list.
  else if ( log_line >= MAX_LOG_LINES )
  {
    // Adjust the linked list.
    backlog_struct_t *tmp_log = oldestlog;
    oldestlog = (backlog_struct_t *)tmp_log->next_logmesg;

    // The oldest log message has no forebears.
    if ( NULL != oldestlog )
    {
      oldestlog->prev_logmesg = NULL;
    }

    // Remove all traces of the discarded log entry
    vPortFree(tmp_log->log_mesg);
    tmp_log->log_mesg = NULL;
    vPortFree(tmp_log);
    tmp_log = NULL;
  }

  // Finally, prepare to prepend to the list
  log_entry->prev_logmesg = latestlog;

  // Ensuring we don't error on null pointers.
  if ( NULL != latestlog )
  {
    latestlog->next_logmesg = log_entry;
    //printf("latestlog->line_number: %4d. latestlog: %08x, latestlog->prev_logmsg: %08x, latestlog->next_logmesg: %08x\n",
    //  latestlog->line_number, (uint32_t)latestlog, (uint32_t) latestlog->prev_logmesg, (uint32_t)latestlog->next_logmesg);
  }
  // Replace the latestlog with this one
  latestlog = log_entry;
  // ------------------------------------------------------------------------------------------
  // - End of section dealing with RAM based logfile.
  // ------------------------------------------------------------------------------------------
}
uint32_t get_log_messages(char **buf, uint32_t *logstart)
{
  char *tmpbuf    = NULL;
  uint16_t buflen = 0;
  uint16_t bufptr = 0;

  // ------------------------------------------------------------------------------------------
  // Search from the latest to the oldest, until we find a log entry older than the log entry
  // we are looking for. Whilst doing this, we'll tally how much RAM we need to allocate.
  // If we get passed '0', we want all the log entries available.
  // ------------------------------------------------------------------------------------------
  backlog_struct_t *logptr = latestlog;
  while ( logptr != NULL )
  {
    if ( (logptr->line_number > *logstart) || (*logstart == 0) )
    {
      buflen += logptr->mesg_len;
    }
    else
    {
      break;
    }
    logptr = (backlog_struct_t *)logptr->prev_logmesg;
  }

  // ------------------------------------------------------------------------------------------
  // Due to our cunning plan, we only need retrace our path from where our previous search
  // ended.
  // ------------------------------------------------------------------------------------------
  if ( buflen > 0 )
  {
    tmpbuf = (char *)pvPortMalloc(buflen);
    if ( NULL == tmpbuf )
    {
      F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating %d bytes for 'tmpbuf'", buflen);
    }
    else
    {
      // Pass the allocated memory address back to the caller.
      (*buf) = tmpbuf;

      // Did we rewind past the last log entry?
      if ( NULL == logptr )
      {
        logptr = oldestlog;
      }
      else
      {
        logptr = (backlog_struct_t *)logptr->next_logmesg;
      }

      while ( NULL != logptr )
      {
        // Safety first
        if ( (bufptr + logptr->mesg_len) <= buflen )
        {
          // Append this log message to the buffer
          memcpy(&tmpbuf[bufptr], logptr->log_mesg, logptr->mesg_len);
          bufptr += logptr->mesg_len;

          // Update the callers record of the most recent log line number
          *logstart = logptr->line_number;
        }
        else
        {
          break;
        }

        logptr = (backlog_struct_t *)logptr->next_logmesg;
      }
    }
  }

  // Assert these should be equal?
  //printf("buflen: %d, bufptr: %d\n", buflen, bufptr);
  //printf("%.*s", buflen, tmpbuf);

  return bufptr;
}
// **************************************************************************************************
// * End of logging code
// **************************************************************************************************


/*********************************************************************************/
/* ============================================================================= */
/* =                    Pseudo Random Number Generators                        = */
/* ============================================================================= */
/*********************************************************************************/
// lib8 functions
// ----------------------------
//int x = random8() % 4;                  // 220us per loop
//int x = random16() % 4;                 // 201us per loop
//int x = random8_max(8) % 4;             // 226us per loop
//int x = random8_min_max(4,24) % 4;      // 263us per loop
//int x = random16_max(8) % 4;            // 240us per loop
//int x = random16_min_max(4,24) % 4;     // 245us per loop

// app_utils functions
// ----------------------------
    //int x = random8();                  // 182us per loop
    //int x = random16();                 // 182us per loop
    //int x = random8_max(8);             // 269us per loop (uint8_t random8_max(uint8_t lim))
    //int x = RANDOM8_MAX(8);             // 182us per loop
    //int x = RANDOM8_MIN_MAX(4, 12);     // 182us per loop
    //int x = RANDOM16_MAX(5);            // 182us per loop
    //int x = RANDOM16_MIN_MAX(24, 48);   // 182us per loop


// The rest during initial testing
// ----------------------------
//int x = esp_random() % 4;               // 710us per loop
//int x = (esp_random() % 4) + 1;         // 710us per loop

//int x = rng64() % 4;                    // 564us per loop
//int x = rng32() % 4;                    // 574us per loop

//int x = xorwow(&xs) % 4;                // 107us per loop (if using local xorwow_state)
//int x = xorwow(&xs) % 4;                // 246us per loop (if using utils.cpp xorwow_state)

//int x = prng() % 4;                     // 264us per loop (backend is xorwow)

//int x = prng() % 4;                     // 392us per loop (backend is xorwow and "prng(xor_state *state=&xs)") \  Using 64bit array
//int x = prng(&xs) % 4;                  // 370us per loop (backend is xorwow and "prng(xor_state *state=&xs)") /

//int x = prng() % 4;                     // 251us per loop (backend is xorwow and "prng(xor_state *state=&xs)") Using union of 32bit and 64bit
//int x = prng(&xs) % 4;                  // 238us per loop (backend is xorwow and "prng(xor_state *state=&xs)") Using union of 32bit and 64bit

//int x = prng() % 4;                     // 439us per loop (backend is xoshiro256ss and "prng(xor_state *state=&xs)")
//int x = prng() % 4;                     // 257us per loop (backend is xorshift64 and "prng(xor_state *state=&xs)")
//int x = prng() % 4;                     // 176us per loop (backend is xorshift32 and "prng(xor_state *state=&xs)")
/*********************************************************************************/
// In this code, there can be only one...
//#define       PRNG_USE_XOSHIRO          true
#define       PRNG_USE_XORSHIFT32       true
//#define       PRNG_USE_XORSHIFT64       true
//#define       PRNG_USE_XORWOW           true
IRAM_DATA_ATTR xor_state xs = {};
void prng_seed(void)
{
  xs.u.x[0] = esp_random();
  xs.u.x[1] = esp_random();
  xs.u.x[2] = esp_random();
  xs.u.x[3] = esp_random();
  xs.u.x[4] = esp_random();
  xs.u.x[6] = esp_random();
  xs.u.x[8] = esp_random();
  xs.u.x[9] = esp_random();
}
// -------------------------------------------------------------------------
// =========================================================================
// -------------------------------------------------------------------------
#if defined (PRNG_USE_XOSHIRO)
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint64_t IRAM_ATTR rol64(uint64_t x, int k)
#else
uint64_t rol64(uint64_t x, int k)
#endif
{
	return (x << k) | (x >> (64 - k));
}
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint64_t IRAM_ATTR xoshiro256ss(struct xor_state *state)
#else
uint64_t xoshiro256ss(struct xor_state *state)
#endif
{
	uint64_t *s = state->u.y;
	uint64_t const result = rol64(s[1] * 5, 7) * 9;
	uint64_t const t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;
	s[3] = rol64(s[3], 45);

	return result;
}
// End of PRNG_USE_XOSHIRO
// -------------------------------------------------------------------------
// =========================================================================
// -------------------------------------------------------------------------
#elif defined (PRNG_USE_XORSHIFT32)
/* The state must be initialized to non-zero */
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint32_t IRAM_ATTR xorshift32(struct xor_state *state)
#else
uint32_t xorshift32(struct xor_state *state)
#endif
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = state->u.x[0];
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return state->u.x[0] = x;
}
#elif defined (PRNG_USE_XORSHIFT64)
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint64_t IRAM_ATTR xorshift64(struct xor_state *state)
#else
uint64_t xorshift64(struct xor_state *state)
#endif
{
	uint64_t x = state->u.y[0];
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	return state->u.y[0] = x;
}
// End of PRNG_USE_XORSHIFT
// -------------------------------------------------------------------------
// =========================================================================
// -------------------------------------------------------------------------
#elif defined (PRNG_USE_XORWOW)
/* The state array must be initialized to not be all zero in the first four words */
// This performs well, but fails a few tests in BigCrush.[5]
// This generator is the default in Nvidia's CUDA toolkit.[6]
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint32_t IRAM_ATTR xorwow(struct xor_state *state)
#else
uint32_t xorwow(struct xor_state *state)
#endif
{
    /* Algorithm "xorwow" from p. 5 of Marsaglia, "Xorshift RNGs" */
    uint32_t t  = state->u.x[4];

    uint32_t s  = state->u.x[0];  /* Perform a contrived 32-bit shift. */
    state->u.x[4] = state->u.x[3];
    state->u.x[3] = state->u.x[2];
    state->u.x[2] = state->u.x[1];
    state->u.x[1] = s;

    t ^= t >> 2;
    t ^= t << 1;
    t ^= s ^ (s << 4);
    state->u.x[0] = t;
    state->counter += 362437;
    return t + state->counter;
}
#endif // End of PRNG_USE_XORWOW
// -------------------------------------------------------------------------
// =========================================================================
// -------------------------------------------------------------------------
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
uint32_t IRAM_ATTR prng(xor_state *state)
#else
uint32_t prng(xor_state *state)
#endif
{
#if defined (PRNG_USE_XOSHIRO)
  return xoshiro256ss(state);
#elif defined (PRNG_USE_XORSHIFT32)
  return xorshift32(state);
#elif defined (PRNG_USE_XORSHIFT64)
  return xorshift64(state);
#elif defined (PRNG_USE_XORWOW)
  return xorwow(state);
#endif
}
// These functions don't need to be trimmed to the desired bit length
// -------------------------------------------------------------------
uint_fast8_t IRAM_ATTR random8 (void)
{
  return xorshift32(&xs);
}
int_fast16_t IRAM_ATTR random16 (void)
{
  return xorshift32(&xs);
}
/*********************************************************************************/
/* ============================================================================= */
/* =                        End of pseudo RNG section                          = */
/* ============================================================================= */
/*********************************************************************************/

/*********************************************************************************/
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR const char *get_filename_ext (const char *filename)
#else
const char *get_filename_ext (const char *filename)
#endif
{
  const char *dot = strrchr(filename, '.');

  if ( !dot || dot == filename )
  {
    return "";
  }
  return dot + 1;
}

/*********************************************************************************/
/* Dump hex (and ASCII), for debugging                                           */
/*********************************************************************************/
#define HEXDUMP_BUFFER  512
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void hexDump(const char *desc, const void *addr, uint16_t len, uint16_t perLine)
#else
void hexDump(const char *desc, const void *addr, uint16_t len, uint16_t perLine)
#endif
{
  char lineBuffer[HEXDUMP_BUFFER + 1] = {};
  char ascBuff[perLine + 1];
  const unsigned char *pc = ( const unsigned char * )addr;
  int bufptr = 0;
  int i = 0;

  // Output description if given.
  if ( desc != NULL )
  {
    bufptr = ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER, "%s:", desc);
  }

  // Length checks.
  if ( len == 0 )
  {
    bufptr += ( int )snprintf(lineBuffer, HEXDUMP_BUFFER - bufptr, "  ZERO LENGTH");
    F_LOGI(true, true, LC_WHITE, lineBuffer);
  }
  else
  {
    F_LOGI(true, true, LC_WHITE, lineBuffer);

    // Silently modify large sizes
    if ( len > 2048 )
    {
      len = 2048;
    }

    // Process every byte in the data.
    for ( bufptr = 0; i < len; i++ )
    {
      int cp = i % perLine;   // Character position
      if ( cp == 0 )
      {
        bufptr += ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "0x%06x: ", i);
        //bufptr += (int)snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "%06d: ", i);
      }

      bufptr += ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "%02x ", pc[i]);
      if ( (pc[i] < 0x20) || (pc[i] > 0x7e) )
      {
        ascBuff[cp] = '.';
      }
      else
      {
        ascBuff[cp] = pc[i];
      }

      if ( cp == (perLine - 1) )
      {
        ascBuff[cp + 1] = 0x0;
        bufptr += ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "  %s", ascBuff);
        F_LOGI(true, true, LC_WHITE, lineBuffer);
        bufptr = 0;
      }
      }

    ascBuff[i % perLine] = 0x0;
    while ( (i % perLine) != 0 )
    {
      bufptr += ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "   ");
      i++;
    }
    bufptr += ( int )snprintf(&lineBuffer[bufptr], HEXDUMP_BUFFER - bufptr, "  %s", ascBuff);

    // And print the final ASCII buffer.
    F_LOGI(true, true, LC_WHITE, lineBuffer);
  }
}

/*********************************************************************************/
/* hex2int take a hex string and convert it to a 32bit number (max 8 hex digits) */
/*********************************************************************************/
#if defined (CONFIG_APPTRACE_SV_ENABLE)
uint32_t hex2int(char *hex)
#else
IRAM_ATTR uint32_t hex2int(char *hex)
#endif
{
  uint32_t val = 0;

  while ( *hex )
  {
    // get current character then increment
    uint8_t byte = *hex++;

    // transform hex character to the 4bit equivalent number, using the ascii table indexes
    if ( byte >= '0' && byte <= '9' )
    {
      byte = byte - '0';
    }
    else if ( byte >= 'a' && byte <= 'f' )
    {
      byte = byte - 'a' + 10;
    }
    else if ( byte >= 'A' && byte <= 'F' )
    {
      byte = byte - 'A' + 10;
    }

    // shift 4 to make space for new digit, and add the 4 bits of the new digit
    val = (val << 4) | (byte & 0xF);
  }

  return val;
}

/*********************************************************************************/
/*  Scan until different or until the end of both (CASE INSENSITIVE)             */
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR int str_cmp(const char *arg1, const char *arg2)
#else
int str_cmp(const char *arg1, const char *arg2)
#endif
{
  int  chk;

  while ( *arg1 || *arg2 )
  {
    if ( (chk = (*arg1++ | 32) - (*arg2++ | 32)) )
    {
      return (chk);
    }
  }

  return ((*arg1++ | 32) - (*arg2++ | 32));
}

/*********************************************************************************/
/* Scan while arg1 is greater than ' ' and arg1 == arg2 (CASE INSENSITIVE)       */
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR int shrt_cmp(const char *arg1, const char *arg2)
#else
int shrt_cmp(const char *arg1, const char *arg2)
#endif
{
  int chk = 0;

  while ( *arg1 > ' ' )
  {
    if ( (chk = (*arg1++ | 32) - (*arg2++ | 32)) )
    {
      return(chk);
    }
  }

  return(0);
}

/*********************************************************************************/
/* Converts a hex character to its integer value                                 */
/* http://www.geekhideout.com/urlcode.shtml                                      */
/*********************************************************************************/
IRAM_ATTR char from_hex(char ch)
{
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/*********************************************************************************/
/* Converts an integer value to its hex character                                */
/*********************************************************************************/
IRAM_ATTR char to_hex(char code)
{
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint16_t url_encode(char *dstBuffer, const char *str, uint16_t buflen)
#else
uint16_t url_encode(char *dstBuffer, const char *str, uint16_t buflen)
#endif
{
  const char *pstr = str;
  char *pbuf = dstBuffer;
  uint16_t i = 0;

  while ( *pstr && ++i < buflen )
  {
    if ( isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' )
    {
      *pbuf++ = *pstr;
    }
    else if ( *pstr == ' ' )
    {
      *pbuf++ = '+';
    }
    else
    {
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
    }
    pstr++;
  }

  *pbuf = '\0';

  return i;
}

/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/
IRAM_ATTR char *binary(char *buf, uint16_t bufSize, unsigned int val)
{
  // Must be able to store one character at least.
  if ( !bufSize )
  {
    return NULL;
  }

  // Work from the end of the buffer back.
  char *pbuf = buf + bufSize;
  *pbuf = '\0';

  // For each bit (going backwards) store character.
  do
  {
    *--pbuf = ((val & 1) == 1) ? '1' : '0';
    val >>= 1;
  }
  while ( (val || ((pbuf - buf) % 8)) && pbuf > buf );

  return pbuf;
}

/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/
IRAM_ATTR uint16_t url_decode(char *dstBuffer, const char *strURL, uint16_t buflen)
{
  const char *pstr = strURL;
  char *pbuf = dstBuffer;
  uint16_t i = 0;

  while ( *pstr && ++i < buflen )
  {
    if ( *pstr == '%' )
    {
      if ( pstr[1] && pstr[2] )
      {
        *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    }
    else if ( *pstr == '+' )
    {
      *pbuf++ = ' ';
    }
    else
    {
      *pbuf++ = *pstr;
    }
    pstr++;
  }

  *pbuf = '\0';

  return i;
}

/*********************************************************************************/
/*********************************************************************************/
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR const char *uri_arg(struct yuarel_param params[], int parts, const char *key)
#else
const char *uri_arg(struct yuarel_param params[], int parts, const char *key)
#endif
{
  const char *retval = NULL;
  int part = parts;

  while ( part-- > 0 )
  {
    if ( !str_cmp(params[part].key, key) )
    {
      return params[part].val;
    }
  }

  return retval;
}

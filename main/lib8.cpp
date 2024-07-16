
// This is based on, if not wholly taken from the FastLEd library.
//
// I did this for two reasons:
// 1. Easily use FastLed patterns in my code.
// 2. In most cases, faster than other ways of doing the same thing.

#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <soc/rmt_struct.h>
#include <lwip/apps/sntp.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_task_wdt.h>


#include <lwip/err.h>
#include <lwip/sys.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <stdio.h>

#include "app_main.h"
#include "app_utils.h"
#include "newlib8.h"
#include "lib8.h"
#include "ws2812_driver.h"


#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint16_t beat88 (accum88 beats_per_minute_88, uint32_t timebase)
#else
uint16_t beat88 (accum88 beats_per_minute_88, uint32_t timebase)
#endif
{
  // BPM is 'beats per minute', or 'beats per 60000ms'.
  // To avoid using the (slower) division operator, we
  // want to convert 'beats per 60000ms' to 'beats per 65536ms',
  // and then use a simple, fast bit-shift to divide by 65536.
  //
  // The ratio 65536:60000 is 279.620266667:256; we'll call it 280:256.
  // The conversion is accurate to about 0.05%, more or less,
  // e.g. if you ask for "120 BPM", you'll get about "119.93".
  return (((clock_ms ()) - timebase) * beats_per_minute_88 * 280) >> 16;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint16_t beat16 (accum88 beats_per_minute, uint32_t timebase)
#else
uint16_t beat16 (accum88 beats_per_minute, uint32_t timebase)
#endif
{
  // Convert simple 8-bit BPM's to full Q8.8 accum88's if needed
  if (beats_per_minute < 256)
    beats_per_minute <<= 8;

  return beat88 (beats_per_minute, timebase);
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t beat8 (accum88 beats_per_minute, uint32_t timebase)
#else
uint8_t beat8 (accum88 beats_per_minute, uint32_t timebase)
#endif
{
  return beat16 (beats_per_minute, timebase) >> 8;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint16_t beatsin88 (accum88 beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
#else
uint16_t beatsin88 (accum88 beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
#endif
{
  uint16_t beat = beat88 (beats_per_minute_88, timebase);
  uint16_t beatsin = (sin16 (beat + phase_offset) + 32768);
  uint16_t rangewidth = highest - lowest;
  uint16_t scaledbeat = scale16 (beatsin, rangewidth);
  uint16_t result = lowest + scaledbeat;

  return result;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint16_t beatsin16 (accum88 beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
#else
uint16_t beatsin16 (accum88 beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset)
#endif
{
  uint16_t beat = beat16 (beats_per_minute, timebase);
  uint16_t beatsin = (sin16 (beat + phase_offset) + 32768);
  uint16_t rangewidth = highest - lowest;
  uint16_t scaledbeat = scale16 (beatsin, rangewidth);
  uint16_t result = lowest + scaledbeat;

  return result;
}

#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t beatsin8 (accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase, uint8_t phase_offset)
#else
uint8_t beatsin8 (accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase, uint8_t phase_offset)
#endif
{
  uint8_t beat = beat8 (beats_per_minute, timebase);
  uint8_t beatsin = sin8 (beat + phase_offset);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8 (beatsin, rangewidth);
  uint8_t result = lowest + scaledbeat;

  return result;
}

/// triwave8: triangle (sawtooth) wave generator.  Useful for
///           turning a one-byte ever-increasing value into a
///           one-byte value that oscillates up and down.
///
///           input         output
///           0..127        0..254 (positive slope)
///           128..255      254..0 (negative slope)
///
/// On AVR this function takes just three cycles.
///
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t triwave8 (uint8_t in)
#else
uint8_t triwave8 (uint8_t in)
#endif
{
  if (in & 0x80)
  {
    in = 255 - in;
  }
  uint8_t out = in << 1;
  return out;
}

/// quadwave8: quadratic waveform generator.  Spends just a little more
///            time at the limits than 'sine' does.
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t quadwave8 (uint8_t in)
#else
uint8_t quadwave8 (uint8_t in)
#endif
{
  return ease8InOutQuad(triwave8(in));
}

/// cubicwave8: cubic waveform generator.  Spends visibly more time
///             at the limits than 'sine' does.
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t cubicwave8(uint8_t in)
#else
uint8_t cubicwave8(uint8_t in)
#endif
{
  return ease8InOutCubic(triwave8(in));
}

/// squarewave8: square wave generator.  Useful for
///           turning a one-byte ever-increasing value
///           into a one-byte value that is either 0 or 255.
///           The width of the output 'pulse' is
///           determined by the pulsewidth argument:
///
///~~~
///           If pulsewidth is 255, output is always 255.
///           If pulsewidth < 255, then
///             if input < pulsewidth  then output is 255
///             if input >= pulsewidth then output is 0
///~~~
///
/// the output looking like:
///
///~~~
///     255   +--pulsewidth--+
///      .    |              |
///      0    0              +--------(256-pulsewidth)--------
///~~~
///
/// @param in
/// @param pulsewidth
/// @returns square wave output
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t squarewave8 (uint8_t in, uint8_t pulsewidth)
#else
uint8_t squarewave8 (uint8_t in, uint8_t pulsewidth)
#endif
{
  if (in < pulsewidth || (pulsewidth == 255))
  {
    return 255;
  }
  else
  {
    return 0;
  }
}

///////////////////////////////////////////////////////////////////////
//
// easing functions; see http://easings.net
//

/// ease8InOutQuad: 8-bit quadratic ease-in / ease-out function
///                Takes around 13 cycles on AVR
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR uint8_t ease8InOutQuad (uint8_t i)
#else
uint8_t ease8InOutQuad (uint8_t i)
#endif
{
  uint8_t j = i;
  if (j & 0x80)
  {
    j = 255 - j;
  }
  uint8_t jj = scale8 (j, j);
  uint8_t jj2 = jj << 1;
  if (i & 0x80)
  {
    jj2 = 255 - jj2;
  }
  return jj2;
}

/// ease8InOutCubic: 8-bit cubic ease-in / ease-out function
///                 Takes around 18 cycles on AVR
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR fract8 ease8InOutCubic (fract8 i)
#else
fract8 ease8InOutCubic (fract8 i)
#endif
{
  uint8_t ii = scale8_LEAVING_R1_DIRTY (i, i);
  uint8_t iii = scale8_LEAVING_R1_DIRTY (ii, i);

  uint16_t r1 = (3 * (uint16_t)(ii)) - (2 * (uint16_t)(iii));

  /*
   * the code generated for the above *'s automatically
   * cleans up R1, so there's no need to explicitily call
   * cleanup_R1();
   */

  uint8_t result = r1;

  // if we got "256", return 255:
  if (r1 & 0x100)
  {
    result = 255;
  }
  return result;
}

/// ease8InOutApprox: fast, rough 8-bit ease-in/ease-out function
///                   shaped approximately like 'ease8InOutCubic',
///                   it's never off by more than a couple of percent
///                   from the actual cubic S-curve, and it executes
///                   more than twice as fast.  Use when the cycles
///                   are more important than visual smoothness.
///                   Asm version takes around 7 cycles on AVR.
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR fract8 ease8InOutApprox (fract8 i)
#else
fract8 ease8InOutApprox (fract8 i)
#endif
{
  if (i < 64)
  {
    // start with slope 0.5
    i /= 2;
  }
  else if (i > (255 - 64))
  {
    // end with slope 0.5
    i = 255 - i;
    i /= 2;
    i = 255 - i;
  }
  else
  {
    // in the middle, use slope 192/128 = 1.5
    i -= 64;
    i += (i / 2);
    i += 32;
  }

  return i;
}

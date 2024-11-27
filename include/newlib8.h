
#ifndef __NEWLIB8_H__
#define __NEWLIB8_H__

#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "ws2812_driver.h"

/// ANSI unsigned short _Fract.  range is 0 to 0.99609375
///                 in steps of 0.00390625
typedef uint8_t fract8;         ///< ANSI: unsigned short _Fract

///  ANSI: signed short _Fract.  range is -0.9921875 to 0.9921875
///                 in steps of 0.0078125
typedef int8_t sfract7;         ///< ANSI: signed   short _Fract

///  ANSI: unsigned _Fract.  range is 0 to 0.99998474121
///                 in steps of 0.00001525878
typedef uint16_t fract16;       ///< ANSI: unsigned       _Fract

// **************************************************
// ** Modified lib8 functions
// **************************************************
#define       abs8(x)       abs(x)                                      // Can't beat the library version (which is inline)

#define       qadd7(x,y)    ((((x)+(y)) > 127) ? 127 : ((x)+(y)))
#define       qadd8(x,y)    ((((x)+(y)) > 255) ? 255 : ((x)+(y)))
#define       qsub8(x,y)    ((((x)-(y)) < 0) ? 0 : ((x)-(y)))

#define       add8(x,y)     (uint8_t)((x)+(y))

#define       sub8(x,y)     (uint8_t)((uint8_t)(x)-(uint8_t)(y))

//#define       avg7(x,y)     (uint8_t)((uint8_t)(x >> 1) + (uint8_t)(y >> 1) + (uint8_t)(x & 0x1))
#define       avg8(x,y)     (uint8_t)((uint8_t)(((x)+(y))>>1))
#define       avg16(x,y)    (uint16_t)((uint32_t)(((uint16_t)(x)+(uint16_t)(y))>>1))

#define       scale8(x,y)   (uint8_t)((uint16_t)(((x) * (y)) >> 8))
#define       scale16(x,y)  (uint16_t)((uint32_t)(((x) * (y)) >> 16))

#define       add8to16(x,y) (uint16_t)((uint8_t)(x)+(uint16_t)(y))

inline __attribute__((always_inline)) uint8_t avg7(uint8_t x, uint8_t y) { return ( (x >> 1) + (y >> 1) + (x & 0x1)); }

cRGB &nblend(cRGB &colA, cRGB &colB, fract8 mixVal);


#endif // __NEWLIB8_H__
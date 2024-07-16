#ifndef __WS2812_DRIVER_H__
#define __WS2812_DRIVER_H__

#include <stdint.h>
#include <esp_err.h>
#include <hal/gpio_types.h>

#define     FAST_HSV       true

/*
typedef union
{
  struct
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
  };
  uint8_t colors[4];
}  __attribute__((packed)) cRGB;
*/

typedef struct
{
  uint8_t     r, g, b;
} cRGB;

typedef struct
{
  float       h, s, l;
} cHSL;

typedef struct
{
  uint8_t     c, m, y, k;
} cCMYK;

#if defined (FAST_HSV)
typedef struct
{
  uint8_t     h, s, v;
} cHSV;
#else
typedef struct
{
  float      h, s;
  uint8_t     v;
} cHSV;
#endif

esp_err_t ws2812_init(gpio_num_t gpioNum, uint16_t num_leds );
esp_err_t ws2812_setColors( uint16_t num_leds, cRGB * array );

inline cRGB toCRGB( float r, float g, float b )
{
  cRGB  v;
  v.r = r;
  v.g = g;
  v.b = b;
  return v;
}

inline cRGB toRGBPixel( uint8_t r, uint8_t g, uint8_t b )
{
  cRGB  v;
  v.r = r;
  v.g = g;
  v.b = b;
  return v;
}

typedef struct
{
  cRGB        entries[16];
} CRGBPalette16;

#endif /* WS2812_DRIVER_H */

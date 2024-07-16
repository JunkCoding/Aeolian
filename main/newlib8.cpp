
// Essential
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "app_utils.h"
#include "newlib8.h"
#include "ws2812_driver.h"

// **************************************************************************************************
// * Optimised for ESP32, lib8 functions
// **************************************************************************************************


/* =======================================================
  In testing, my test code ran with the followibng timings:
  1. Using doubles: 10,000us in code over 1,000 iterations.
  2. Using floats: 7,000us in code over 1,000 iterations.
  3. Using uint_fast32_t: 1,080us over 1,000 iterations.
========================================================== */
/*
cCMYK rgb_to_cmyk(cRGB cRGB, cCMYK &cCMYK)
{
  float ctmp = 1.0 - ((float)cRGB.r / 255.0);
  float mtmp = 1.0 - ((float)cRGB.g / 255.0);
  float ytmp = 1.0 - ((float)cRGB.b / 255.0);
  float ktmp = MIN(MIN(ctmp, mtmp), ytmp);

  // Standard CMYK using: 0-100%, 0-100%, 0-100%, 0-100%
  if ( ktmp == 1.0 )
  {
    cCMYK = {0,0,0,0};
  }
  else
  {
    ctmp = (ctmp - ktmp) / (1.0 - ktmp);
    mtmp = (mtmp - ktmp) / (1.0 - ktmp);
    ytmp = (ytmp - ktmp) / (1.0 - ktmp);
  }

  cCMYK.c = (float)ctmp * 100;
  cCMYK.m = (float)mtmp * 100;
  cCMYK.y = (float)ytmp * 100;
  cCMYK.k = (float)ktmp * 100 + 0.5;

  return cCMYK;
}
cRGB cmyk_to_rgb(cCMYK cCMYK, cRGB &cRGB)
{
  float k = 1 - (float)cCMYK.k / 100;
  cRGB.r = 255 * (1 - (float)cCMYK.c / 100) * k;
  cRGB.g = 255 * (1 - (float)cCMYK.m / 100) * k;
  cRGB.b = 255 * (1 - (float)cCMYK.y / 100) * k;

  return cRGB;
}
*/
// Fast RGB to CMYK (representation)
cCMYK rgb_to_cmyk(cRGB cRGB, cCMYK &cCMYK)
{
  uint_fast32_t ctmp = 255 - cRGB.r;
  uint_fast32_t mtmp = 255 - cRGB.g;
  uint_fast32_t ytmp = 255 - cRGB.b;
  uint_fast32_t ktmp = MIN(MIN(ctmp, mtmp), ytmp);

  if ( ktmp == 255 )
  {
    cCMYK = {0,0,0,0};
  }
  else
  {
    cCMYK.c = (100 * (ctmp - ktmp)) / (255 - ktmp);
    cCMYK.m = (100 * (mtmp - ktmp)) / (255 - ktmp);
    cCMYK.y = (100 * (ytmp - ktmp)) / (255 - ktmp);
  }
  cCMYK.k = 100 * ktmp / 255;
  return cCMYK;
}
cRGB cmyk_to_rgb(cCMYK cCMYK, cRGB &cRGB)
{
  uint_fast32_t k = 100 - cCMYK.k;

  cRGB.r = ((255 - ((255 * cCMYK.c) / 100)) * k) / 100;
  cRGB.g = ((255 - ((255 * cCMYK.m) / 100)) * k) / 100;
  cRGB.b = ((255 - ((255 * cCMYK.y) / 100)) * k) / 100;
  return cRGB;
}

cRGB &nblend(cRGB &colA, cRGB &colB, fract8 mixVal)
{
  cCMYK colC = {0,0,0,0};
  cCMYK colD = {0,0,0,0};

  rgb_to_cmyk(colA, colC);
  rgb_to_cmyk(colB, colD);

  cCMYK colE = {MAX(colC.c, colD.c), MAX(colC.m, colD.m), MAX(colC.y, colD.y), MIN(colC.k, colD.k)};
  cmyk_to_rgb(colE, colA);

  return colA;
}

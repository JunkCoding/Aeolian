


#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <soc/rmt_struct.h>
#include <esp_task_wdt.h>
#include <esp_err.h>

#include <math.h>

#include "app_main.h"
#include "newlib8.h"
#include "lib8.h"
#include "ws2812_driver.h"
#include "app_lightcontrol.h"
#include "patterns.h"
#include "colours.h"
#include "app_sntp.h"
#include "app_utils.h"

#define FPS   (1000000 / CONFIG_LED_LOOP_US)

#if defined (FAST_HSV)
_hue_t hues[] = {
  {0,   42},
  {43,  85},
  {86,  127},
  {128, 170},
  {171, 213},
  {214, 255},
  {0,   255},
};
#else  // FAST_HSV
/*
 * Red falls between 0 and 60 degrees.
 * Yellow falls between 61 and 120 degrees.
 * Green falls between 121 and 180 degrees.
 * Cyan falls between 181 and 240 degrees.
 * Blue falls between 241 and 300 degrees.
 * Magenta falls between 301 and 360 degrees.
 */
_hue_t hues[] = {
  {0,   60},
  {61,  120},
  {121, 180},
  {181, 240},
  {241, 300},
  {301, 360},
  {0,   360},
};
#endif // FAST_HSV

uint8_t star_bright[] = {0,0,1,2,3};

// *********************************************************
// * Helper functions
// *********************************************************
int compareColours (cRGB a, cRGB b)
{
  int chk = false;
  if ( a.r == b.r && a.g == b.g && a.b == b.b )
  {
    chk = true;
  }
  return chk;
}

// *********************************************************
// Fast initial change, followed by a slow transition.
// Results in 'fade outs' taking a lot longer than
// 'fade ins', which are almost instantaneous.
// *********************************************************
uint8_t scale8_video(uint8_t i, fract8 scale)
{
  uint8_t v = 0;

  if ( i || scale )
  {
    if ( !(v = scale8(i, scale)) )
    {
      v = 1;
    }
  }

  return v;
}

// *********************************************************
// None linear fade towards a target colour.
// *********************************************************
uint8_t blendToward(uint8_t target, uint8_t cur, uint8_t amount)
{
  uint8_t ret = cur;

  if ( cur < target )
  {
    uint8_t delta = target - cur;
    delta = scale8_video(delta, amount);
    F_LOGI(true, true, LC_GREY, "delta = %d", delta);
    ret += delta;
  }
  else if ( cur > target )
  {
    uint8_t delta = cur - target;
    delta  = scale8_video(delta, amount);
    F_LOGI(true, true, LC_GREY, "delta = %d", delta);
    ret -= delta;
  }

  return ret;
}

// *********************************************************
// Manage the background colour, fading from one to another
// or via black, if appropriate.
// *********************************************************
/*
void set_background (uint16_t start, uint16_t count, cRGB colour, controlvars_t *cvars)
{
  static cRGB req_colour = {};         // Last requested colour
  static cRGB vis_colour = {};         // Actual colour being displayed at this moment

  // Does the passed colour match the current colour?
  if ( compareColours (req_colour, colour) == false )
  {
    // Did we finish cross-fading the last colours?
    if ( compareColours(req_colour, vis_colour) == true )
    {
      if ( compareColours (vis_colour, CRGBBlack) == true )
      {
        req_colour = colour;
      }
      else
      {
        req_colour = CRGBBlack;
      }
    }
  }

  // Are the lights set to the last requested colour?
  if ( compareColours (req_colour, vis_colour) == false )
  {
    vis_colour.r = blendToward(req_colour.r, vis_colour.r, 15);
    vis_colour.g = blendToward(req_colour.g, vis_colour.g, 15);
    vis_colour.b = blendToward(req_colour.b, vis_colour.b, 15);

    printf ("vis_colour: %02X%02X%02X, req_colour: %02X%02X%02X\n",
      vis_colour.r, vis_colour.g, vis_colour.b,
      req_colour.r, req_colour.g, req_colour.b);
  }

  rgbFillSolid (start, count, vis_colour, false, cvars->dim);
}
*/

/*
// *********************************************************
// Linear RGB method.
// *********************************************************
void set_background (uint16_t start, uint16_t count, cRGB colour, controlvars_t *cvars)
{
  static cRGB req_colour = {};         // Last requested colour
  static cRGB old_colour = {};         // Colour we are migrating from
  static cRGB vis_colour = {};         // Actual colour being displayed at this moment
  static uint8_t step = 0;
  static int16_t dr = 0, dg = 0, db = 0;

  // Does the passed colour match the current colour?
  if ( compareColours (req_colour, colour) == false )
  {
    // Did we finish cross-fading the last colours?
    if ( compareColours (req_colour, vis_colour) == true )
    {
      old_colour = vis_colour;
      if ( compareColours(vis_colour, CRGBBlack) == true )
      {
        req_colour = colour;
      }
      else
      {
        req_colour = CRGBBlack;
      }
      step = 0;

      // Calculate the increment/decrement to reach the next colour
      dr = req_colour.r - old_colour.r;
      dg = req_colour.g - old_colour.g;
      db = req_colour.b - old_colour.b;
    }
  }

  // Are the lights set to the last requested colour?
  if ( compareColours (req_colour, vis_colour) == false )
  {
    vis_colour.r = old_colour.r + (step * dr / 20);
    vis_colour.g = old_colour.g + (step * dg / 20);
    vis_colour.b = old_colour.b + (step * db / 20);
    step++;

    //printf ("vis_colour: %02X%02X%02X, req_colour: %02X%02X%02X\n",
    //  vis_colour.r, vis_colour.g, vis_colour.b,
    //  req_colour.r, req_colour.g, req_colour.b);
  }

  rgbFillSolid (start, count, vis_colour, false, cvars->dim);
}*/

// *********************************************************
// Linear RGB method.
// *********************************************************
void set_background (uint16_t start, uint16_t count, cRGB colour, controlvars_t *cvars)
{
  static cRGB req_colour = {};         // Last requested colour
  static cRGB old_colour = {};         // Colour we are migrating from
  static cRGB vis_colour = {};         // Actual colour being displayed at this moment

  // Does the passed colour match the current colour?
  if ( compareColours (req_colour, colour) == false )
  {
    // Did we finish cross-fading the last colours?
    if ( compareColours (req_colour, vis_colour) == true )
    {
      old_colour = vis_colour;
      if ( compareColours (vis_colour, CRGBBlack) == true )
      {
        req_colour = colour;
      }
      else
      {
        req_colour = CRGBBlack;
      }
    }
  }

  // Are the lights set to the last requested colour?
  if ( compareColours (req_colour, vis_colour) == false )
  {
    cHSV col = rgb2hsv(vis_colour);

    //printf ("vis_colour: %02X%02X%02X, req_colour: %02X%02X%02X\n",
    //  vis_colour.r, vis_colour.g, vis_colour.b,
    //  req_colour.r, req_colour.g, req_colour.b);
  }

  rgbFillSolid (start, count, vis_colour, false, cvars->dim);
}

// *********************************************************
uint16_t next_quadrant_pixel (uint16_t cur_pixel, uint16_t start, uint16_t count, bool dec)
{
  if ( dec )
  {
    return dec_curpos (cur_pixel, start, count);
  }
  else
  {
    return inc_curpos(cur_pixel, start, count);
  }
}

// *********************************************************
uint16_t next_linear_pixel (uint16_t cur_pixel, uint16_t count, bool dec)
{
  if ( dec )
  {
    if ( cur_pixel == 0 )
      cur_pixel = count - 1;
    else
      cur_pixel--;
  }
  else
  {
    cur_pixel++;
    if ( cur_pixel >= count )
      cur_pixel = 0;
  }

  return cur_pixel;
}

/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle (int *array, size_t n)
{
  if ( n > 1 )
  {
    size_t i;
    for ( i = 0; i < n - 1; i++ )
    {
      size_t j = i + rand () / (RAND_MAX / (n - i) + 1);
      int t = array[j];
      array[j] = array[i];
      array[i] = t;
    }
  }
}

// Just light a random pixel each iteration
// *********************************************************
void circular (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static int fade_delay = 0;
  static int pixel_delay = 0;
  static uint16_t curPos = 5555;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_CIRCULAR;

  if ( --fade_delay < 1 )
  {
    // Fade before we run the code
    rgbFadeAll (start, count, (prng() % 7) + 1);

    // Reset fade counter
    fade_delay = cvars->pixel_count;
  }
  else
  {
    // outBuffer was cleared, so we need to restore it from the rgbBuffer
    restorePaletteAll ();
  }

  if ( --pixel_delay < 1 )
  {
    curPos = inc_curpos(curPos, start, count);

    cHSV pixel = getPixelHSV (curPos);
    if ( pixel.v < 25 )
    {
      setPixelRGB(curPos, rgbFromPalette (curPalette[cvars->thisPalette], prng() & 0xF), true, cvars->dim);
    }

    // Delay until our next pixel/fade
    pixel_delay = 0;
  }
}
// *********************************************************
bool get_interleave (uint8_t *width, uint8_t *interleave, CRGBPalette16 palette)
{
  bool noErr = true;
  uint8_t x = 0;
  cRGB pixelx = {};
  cRGB pixela = {};
  cRGB pixelb = {};

  *width = 0;
  *interleave = 1;

  // Save this colour. If we see it again, after finding
  // an alternative colour, we stop further processing.
  pixelx = rgbFromPalette (palette, x);

  // Find out if we have interleaved light colours
  do
  {
    pixela = rgbFromPalette (palette, x);

    uint8_t owidth = 1;
    for ( x = x + 1; x < 16; x++ )
    {
      // Get the next colouir. Saved to var for ease of use.
      pixelb = rgbFromPalette (palette, x);
      if ( compareColours(pixela, pixelb) == true )
      {
        owidth++;
      }
      else
      {
        break;
      }
    }

    if ( *width == 0 )
    {
      *width = owidth;
    }
    else if ( *width != owidth )
    {
      // Uneven spacing, reject fancy methods
      *width = 1;
      *interleave = 0;
      break;
    }

    // Update the interleave;
    if ( compareColours (pixelx, pixelb) == true )
    {
      (*interleave)++;
    }
  }
  while ( x < 16 );

  return noErr;
}

// *********************************************************
// * Fade all lights in and out individually
// *********************************************************
#define FAO_NIL   0
#define FAO_DEC   1
#define FAO_INC   2
#define FAO_SPEED 25
#define FAO_MIN   8
#define FAO_RAND  8
typedef struct
{
  cHSV    pixel;
  uint8_t fading;
  uint8_t speed;
} _pixel_control;
void fadeinandout (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static cHSV last_pixel;
  uint16_t static pixel_count = 0;
  static _pixel_control *pixel_control = NULL;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_FADEINANDOUT;

  if ( pixel_count != count )
  {
    pixel_count = count;

    // Check if we have a previously allocated control array and release it
    if ( pixel_control )
    {
      vPortFree (pixel_control);
      pixel_control = NULL;
    }

    // Allocate our control array
    pixel_control = (_pixel_control *)pvPortMalloc (pixel_count * sizeof (_pixel_control));
    if ( pixel_control == NULL )
    {
      F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'pixel_control'", (pixel_count * sizeof (_pixel_control)));
      return;
    }

    for ( uint16_t i = 0; i < pixel_count; i++ )
    {
      pixel_control[i].fading = FAO_NIL;
    }
  }

  uint16_t curPos = start;
  for ( uint16_t i = 0; i < count; i++ )
  {
    // Needs initialising?
    if ( pixel_control[curPos].fading == FAO_NIL )
    {
      pixel_control[curPos].pixel   = rgb2hsv(rgbFromPalette(curPalette[cvars->thisPalette], prng() & 0xF));
      pixel_control[curPos].pixel.v = 120;
      pixel_control[curPos].fading  = 1 + (prng() % 2);
      pixel_control[curPos].speed   = 1 + (prng() % FAO_SPEED);
    }
    // Process descending / ascending
    else
    {
      if ( pixel_control[curPos].fading == FAO_DEC )
      {
        if ( (pixel_control[curPos].pixel.v - pixel_control[curPos].speed) > FAO_MIN )
        {
          pixel_control[curPos].pixel.v -= pixel_control[curPos].speed;
        }
        else if ( pixel_control[curPos].pixel.v == --pixel_control[curPos].speed )
        {
          if ( (prng() % 10) < FAO_RAND )
          {
            pixel_control[curPos].pixel = last_pixel;
          }
          else
          {
            pixel_control[curPos].pixel = rgb2hsv(rgbFromPalette(curPalette[cvars->thisPalette], prng() & 0xF));

          }
          pixel_control[curPos].pixel.v = FAO_MIN;
          pixel_control[curPos].fading  = FAO_INC;
          pixel_control[curPos].speed   = 1 + (prng() % FAO_SPEED);
        }
        else if ( pixel_control[curPos].pixel.v > 0 )
        {
          pixel_control[curPos].pixel.v = 0;
          pixel_control[curPos].speed   = 20 + (prng() % 75);
        }
      }
      else
      {
        if ( (uint16_t)(pixel_control[curPos].pixel.v + pixel_control[curPos].speed) < 255 )
        {
          pixel_control[curPos].pixel.v += pixel_control[curPos].speed;
        }
        else
        {
          pixel_control[curPos].fading = FAO_DEC;
        }
      }
    }

    // Set the pixel
    setPixelHSV (curPos, pixel_control[curPos].pixel, true, cvars->dim);

    // Try and group some colours together
    if ( pixel_control[curPos].pixel.s )
    {
      last_pixel = pixel_control[curPos].pixel;
    }

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);
  }
}

// *********************************************************
// * Bouncing ball pattern
// *********************************************************
//#define BALL_GRAVITY                -9.81
#define BALL_GRAVITY                -0.50
#define BALL_STARTHEIGHT            1
#define BALL_IMPACT_VELOCITY_START  sqrt(-2 * BALL_GRAVITY * BALL_STARTHEIGHT)
#define BALL_MAX                    5
typedef struct
{
  uint8_t  lbc;
  float    Height[BALL_MAX];
  float    ImpactVelocity[BALL_MAX];
  float    TimeSinceLastBounce[BALL_MAX];
  int      Position[BALL_MAX];
  uint64_t ClockTimeSinceLastBounce[BALL_MAX];
  float    Dampening[BALL_MAX];
} _bb_control;
void  bb_proper (uint16_t, uint16_t, uint8_t, controlvars_t *, _bb_control *, uint8_t, cRGB[]);
void bouncingballs (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static _bb_control bb_params = {};
  static cRGB balls[] = {Red, Blue, Green, Orange};

  // Set our pattern delay
  cvars->delay_ticks = DELAY_BOUNCINGBALLS;

  bb_proper (start, count, step, cvars, &bb_params, (sizeof (balls) / sizeof (cRGB)), balls);
}
/*
void bouncingballs(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static _bb_control bb_params[4] = {};
  static cRGB balls[] = { Red, Blue, Green, Orange };

  // Set our pattern delay
  cvars->delay_ticks = DELAY_BOUNCINGBALLS;

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  restorePaletteAll();
  // Fade before we run the code
  rgbFadeAll(start, count, 35);

  for (uint8_t x = 0; x < 4; x++)
  {
    bb_proper(overlay[x].start, overlay[x].count, step, cvars, &bb_params[x], (sizeof(balls) / sizeof(cRGB)), balls);
  }
}
*/
void bb_proper (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars, _bb_control *bb_params, uint8_t bc, cRGB balls[])
{
  if ( bb_params->lbc != bc )
  {
    for ( int i = 0; i < bc; i++ )
    {
      bb_params->ClockTimeSinceLastBounce[i] = mp_hal_ticks_ms ();
      bb_params->Height[i] = BALL_STARTHEIGHT;
      bb_params->Position[i] = 0;
      bb_params->ImpactVelocity[i] = BALL_IMPACT_VELOCITY_START;
      bb_params->TimeSinceLastBounce[i] = 0;
      bb_params->Dampening[i] = 0.90 - (float)(i) / pow (bc, 2);
    }

    bb_params->lbc = bc;
  }

  for ( int i = 0; i < bc; i++ )
  {
    bb_params->TimeSinceLastBounce[i] = mp_hal_ticks_ms () - bb_params->ClockTimeSinceLastBounce[i];
    bb_params->Height[i] = 0.5 * BALL_GRAVITY * pow (bb_params->TimeSinceLastBounce[i] / 1000, 2.0) + bb_params->ImpactVelocity[i] * bb_params->TimeSinceLastBounce[i] / 1000;

    if ( bb_params->Height[i] < 0 )
    {
      bb_params->Height[i] = 0;
      bb_params->ImpactVelocity[i] = bb_params->Dampening[i] * bb_params->ImpactVelocity[i];
      bb_params->ClockTimeSinceLastBounce[i] = mp_hal_ticks_ms ();

      if ( bb_params->ImpactVelocity[i] < 0.01 )
      {
        bb_params->ImpactVelocity[i] = BALL_IMPACT_VELOCITY_START;
      }
    }

    bb_params->Position[i] = round (bb_params->Height[i] * (count - 1) / BALL_STARTHEIGHT);
  }

  for ( int i = 0; i < bc; i++ )
  {
    if ( bb_params->Position[i] < count )
    {
      uint16_t pos = linear2quadrant (bb_params->Position[i], start, count);

      setPixelRGB(pos, balls[i], true, cvars->dim);

      //setPixelRGB(bb_params->Position[i] , balls[i], true, cvars->dim);
    }
  }
}

// *********************************************************
// * Blocks of colour in quadrants
// *********************************************************
void blocky (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t style = 2;
  static cRGB cols[5];
  static bool init = false;
  uint8_t x;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_BLOCKY;

  if ( !init || ((prng() % 1000) < 10) )
  {
    for ( x = 0; x < cvars->num_overlays; x++ )
    {
      cols[x] = rgbFromPalette (curPalette[cvars->thisPalette], prng() & 0xF);
      cvars->delay_ticks = 10 + (prng() % 45);
    }
    init = true;
  }

  switch ( style )
  {
    case 0:
      {
        for ( uint8_t x = 0; x < cvars->num_overlays; x++ )
        {
          rgbFillSolid (overlay[x].zone_params.start, overlay[x].zone_params.count, rgbFromPalette (curPalette[cvars->thisPalette], prng() & 0xF), true, cvars->dim);
        }
      }
      break;
    case 1:
      {
        for ( uint8_t x = 0; x < (cvars->num_overlays / 2); x++ )
        {
          cols[0] = rgbFromPalette (curPalette[cvars->thisPalette], prng() & 0xF);
          rgbFillSolid (overlay[x].zone_params.start, overlay[x].zone_params.count, cols[0], true, cvars->dim);
          rgbFillSolid (overlay[x + 2].zone_params.start, overlay[x + 2].zone_params.count, cols[0], true, cvars->dim);
        }
      }
      break;
    case 2:
      {
        // Save the first colour
        cRGB tc = cols[0];

        for ( uint8_t x = 0; x < cvars->num_overlays; x++ )
        {
          rgbFillSolid (overlay[x].zone_params.start, overlay[x].zone_params.count, cols[x], true, cvars->dim);
          cols[x] = cols[x + 1];
        }

        cols[3] = tc;
      }
      break;
    default:
      break;
  }
}

// *********************************************************
// * Meteor Rain
// *********************************************************
struct meteor_t
{
  uint16_t    curPos;
  uint16_t    size;
  int8_t      spd;
  int8_t      spdctr;
  bool        dir;
  bool        loop;
  cRGB        color;
} meteors[] = {
  {5,       10,     2,    2,    0,    1,    Red},
  {10,      10,     3,    3,    1,    1,    Green},
  {15,      10,     4,    4,    0,    1,    Blue},
  /*
  {20,      10,     2,    2,    0,    1,    Red},
  {25,      10,     3,    3,    1,    1,    Green},
  {30,      10,     4,    4,    0,    1,    Blue},
  */
};
#define MET_COUNT (sizeof(meteors) / sizeof(struct meteor_t))

cRGB primary_colors[] = {Red, Green, Blue};
#define PRI_COUNT (sizeof(primary_colors) / sizeof(cRGB))
void meteor (cRGB *metPalette, struct meteor_t *meteor, uint16_t count, bool ranDecay, uint8_t decayRate)
{
  meteor->spdctr--;
  if ( meteor->spdctr == 0 )
  {
    meteor->spdctr = meteor->spd;

    // Fate to black
    for ( uint16_t i = 0; i < count; i++ )
    {
      if ( !ranDecay || (prng() % 10) > 8 )
      {
        cHSV pixel = rgb2hsv(metPalette[i]);

        if ( pixel.v )
        {
          if ( pixel.v > decayRate )
          {
            pixel.v -= decayRate;
          }
          else
          {
            pixel.s = 0;
            pixel.v = 0;
          }

          metPalette[i] = hsv2rgb(pixel);
        }
      }
    }

    // Display our meteor
    for ( uint16_t j = 0; j < meteor->size; j++ )
    {
      if ( meteor->dir )
      {
        if ( ((meteor->curPos - j) < count) && ((meteor->curPos - j) >= 0) )
        {
          metPalette[(meteor->curPos - j)] = meteor->color;
        }
      }
      else
      {
        if ( ((meteor->curPos + j) < count) && ((meteor->curPos + j) >= 0) )
        {
          metPalette[(meteor->curPos + j)] = meteor->color;
        }
      }
    }

    // Move our meteor
    if ( meteor->dir )
    {
      if ( ++meteor->curPos >= count )
      {
        meteor->curPos = 0;

        if ( (prng() % 25) > 20 )
        {
          meteor->dir = prng() % 1;
          meteor->spd = (prng() % 4) + 1;
          meteor->size = (prng() % (count / 2)) + (count / 3);
          meteor->color = primary_colors[(prng() % PRI_COUNT)];
        }
      }
    }
    else
    {
      if ( meteor->curPos-- == 0 )
      {
        meteor->curPos = count;

        if ( (prng() % 25) > 20 )
        {
          meteor->dir = prng() % 1;
          meteor->spd = (prng() % 4) + 1;
          meteor->size = (prng() % (count / 2)) + (count / 3);
          meteor->color = primary_colors[(prng() % PRI_COUNT)];
        }
      }
    }
  }
}

void meteorRain (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t pixels = 0;
  static cRGB *metPalette = NULL;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_METEOR;

  // Check if we need to allocate our workspace
  if ( pixels != count )
  {
    pixels = count;

    if ( metPalette != NULL )
    {
      vPortFree (metPalette);
      metPalette = NULL;
    }

    metPalette = (cRGB *)pvPortMalloc ((MET_COUNT * count) * sizeof (cRGB));
    if ( metPalette == NULL )
    {
      F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'metPalette'", ((MET_COUNT * count) * sizeof (cRGB)));
      return;
    }
    memset (metPalette, 0, (MET_COUNT * count) * sizeof (cRGB));

    for ( int j = 0; j < MET_COUNT; j++ )
    {
      meteors[j].size = (prng() % (count / 2)) + (count / 5);
    }
  }

  // Loop through our meteors
  for ( int i = 0; i < MET_COUNT; i++ )
  {
    meteor (&metPalette[i * count], &meteors[i], count, false, 15);
  }

  // Loop through our meteors and blend them together
  uint16_t curPos = start;
  for ( uint16_t i = 0; i < count; i++ )
  {
    cRGB tmpColor = {};
    for ( int j = 0; j < MET_COUNT; j++ )
    {
      tmpColor = blendRGB (tmpColor, metPalette[(j * count) + i]);
    }

    //F_LOGI(true, true, LC_GREY, "metPalette[%d] = {%02X,%02X,%02X}", i, metPalette[i].r, metPalette[i].g, metPalette[i].b);
    setPixelRGB(curPos, tmpColor, true, cvars->dim);

    // Increment and loop if necessary
    curPos = inc_curpos(curPos, start, count);
  }
}

float sq (float num)
{
  return num * num;
}

void plasma (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t offset = 0; // counter for radial color wave motion
  static int plasVector = 0; // counter for orbiting plasma center

  // Set our pattern delay
  cvars->delay_ticks = DELAY_PLASMA;

  // Calculate current center of plasma pattern (can be offscreen)
  uint16_t xOffset = cos16 (plasVector / 360) % 360;
  uint16_t yOffset = sin16 (plasVector / 360) % 360;

  // Draw one frame of the animation into the LED array
  uint16_t curPos = start;
  uint16_t y = 10;
  for ( uint16_t x = 0; x < count; x++ )
  {
#if defined (FAST_HSV)
    uint8_t color = abs(sin8(sqrt (sq (((float)x - 7.5) * 10 + xOffset - 180) + sq (((float)y - 2) * 10 + yOffset - 180)) + offset) % 360);
    setPixelHSV (curPos, (cHSV){color, 255, 255}, true, cvars->dim);
#else
    uint16_t color = abs(sin16(sqrt (sq (((float)x - 7.5) * 10 + xOffset - 180) + sq (((float)y - 2) * 10 + yOffset - 180)) + offset) % 360);
    setPixelHSV (curPos, (cHSV) {(float)color, 1, 255}, true, cvars->dim);
#endif

    curPos = inc_curpos(curPos, start, count);

    if ( ++yOffset >= 360 )
    {
      yOffset = 0;
    }
  }

  if ( ++offset >= 360 )
  {
    offset = 0;
  }

  // using an int for slower orbit (wraps at 65536)
  plasVector += 16;
}

// *********************************************************
// * Rachels (ie. Multi tasking 4 patterns)
// *********************************************************
void rachels (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static int pc = 0;      // Count of available patterns to use
  static int *pl = NULL;  // List of pattern numbers

  // Set our pattern delay
  cvars->delay_ticks = DELAY_RACHELS;

  // Initialise our array of available functions
  if ( !pc )
  {
    for ( uint8_t x = 0; x <= control_vars.num_patterns; x++ )
    {
      // Count how many patterns are available to us
      if ( patterns[x].enabled && BTST (patterns[x].mask, MASK_MULTI) )
      {
        pc++;
      }
    }

    // Allocate an array to store the list of available patterns
    if ( pl )
    {
      vPortFree (pl);
      pl = NULL;
    }

    // Allocate our control array
    pl = (int *)pvPortMalloc (pc * sizeof (int));
    if ( pl == NULL )
    {
      F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'pl'", (pc * sizeof(int)));
      return;
    }

    for ( uint8_t x = 0, y = 0; y < pc; x++ )
    {
      if ( patterns[x].enabled && BTST (patterns[x].mask, MASK_MULTI) )
      {
        pl[y++] = x;
      }
    }

    F_LOGW(true, true, LC_BRIGHT_YELLOW, "%d patterns available for 'rachels'", pc);
    for ( uint8_t x = 0; x < cvars->num_overlays; x++ )
    {
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "pl[%d] = %2d, start: %3d, count: %3d (%s)", x, pl[x], overlay[x].zone_params.start, overlay[x].zone_params.count, patterns[pl[x]].name);
    }
  }

#if defined (CONFIG_AEOLIAN_DEBUG_DEV)
  if ( (prng() % 1000) == 1 )
#else
  if ( (prng() % 4000) == 1 )
#endif /* CONFIG_AEOLIAN_DEBUG_DEV */
  {
    shuffle (pl, pc);
#if defined (CONFIG_AEOLIAN_DEBUG_DEV)
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "Shuffle:");
    for ( uint8_t x = 0; x < cvars->num_overlays; x++ )
    {
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "pl[%d] = %2d, start: %3d, count: %3d (%s)", x, pl[x], overlay[x].zone_params.start, overlay[x].zone_params.count, patterns[pl[x]].name);
    }
#endif /* CONFIG_AEOLIAN_DEBUG_DEV */
  }

#if defined (CONFIG_AEOLIAN_DEBUG_DEV)
  uint8_t pat = 2;
  //((*patterns[pat].pointer) (overlay[0].start, overlay[0].count, 0, cvars));
  ((*patterns[pat].pointer) (overlay[1].zone_params.start, overlay[1].zone_params.count, 0, cvars));
  //((*patterns[pat].pointer) (overlay[2].start, overlay[2].count, 0, cvars));
  //((*patterns[pat].pointer) (overlay[3].start, overlay[3].count, 0, cvars));
#else
  for ( uint8_t x = 0; x < cvars->num_overlays; x++ )
  {
    ((*patterns[pl[x]].pointer) (overlay[x].zone_params.start, overlay[x].zone_params.count, 0, cvars));
  }
#endif /* CONFIG_AEOLIAN_DEBUG_DEV */
}

// *********************************************************
// * Null, Nada, Nowt. (good for debugging)
// *********************************************************
void null_pattern (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars __attribute__ ((unused)))
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_NULLPATTERN;

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  restorePalette (start, count);
}

// *********************************************************
// * Solid colour with optional sparklies
// *********************************************************
void solid_colour (uint16_t start, uint16_t count, uint8_t step, cRGB colour, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_DAILY_COLOUR;

  rgbFillSolid (start, count, colour, true, cvars->dim);
}
// *********************************************************
// * NHS Appreciation Pattern
// *********************************************************
void nhs_appreciation (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_NHSBLUE;

  solid_colour (start, count, step, CRGBNHSBlue, cvars);
}
// *********************************************************
// * Welsh Nationalist Pattern
// *********************************************************
void welsh_pride (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_WELSHPRIDE;

  solid_colour (start, count, step, CRGBRed, cvars);
}
// *********************************************************
// * Just a plain solid colour (for now)
// *********************************************************
void daily_colour (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  solid_colour (start, count, step, cvars->static_col, cvars);
}

// *********************************************************
// * Old School
// *********************************************************
void oldschool (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars __attribute__ ((unused)))
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_OLDSCHOOL;

  FillLEDsFromPaletteColors (0, start, count);
}

// *********************************************************
// *********************************************************
void oldschoolRotate (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t o = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_OLDSCHOOL_ROTATE;

  cvars->palette_stretch = 1;
  FillLEDsFromPaletteColors(o, start, count);

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    o++;
  }
  else
  {
    o--;
  }
}

// *********************************************************
// *********************************************************
typedef struct
{
  uint8_t mode;
  uint8_t delay;
  int16_t brightness;
} _interleave;
#define T_SPEED 32
#define T_DELAY 1
#define T_UP    1
#define T_DOWN  2
#define T_BOTH  T_UP + T_DOWN
#define T_MODE  T_BOTH
void traditional (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t aFadeSpd[] = {4,8,16,32,48,64};
  static uint8_t aStepSpd[] = {4,3,2,1};
  static uint8_t last_palette = 255;
  static uint8_t width;
  static uint8_t ilc;
  static uint8_t stp;
  static uint8_t part;
  static uint8_t top = 0;
  static uint8_t fs = 0;
  static uint8_t ss = 0;
  static uint8_t mode = T_MODE;
  static _interleave interleave[8];

  // On our first run or when our palette has changed we check
  // if, and what, pattern this palette is made up of.
  if ( last_palette != cvars->thisPalette )
  {
        // Get the pattern interleave (if there is one)
    get_interleave (&width, &ilc, curPalette[cvars->thisPalette]);

    // Save for next iteration
    //last_palette = cvars->thisPalette;
  }

  // Run this code only when our direction is changed or when we are
  // first run.
  //if (last_dir != BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT))
  if ( last_palette != cvars->thisPalette )
  {
    // Aggregate interleave sets?
    // -------------------------------------
    // ie. Red Green Blue Yellow
    //  1:  |    -    |     -
    //  2:  -    |    -     |
    // Elset, each colour fades seperately
    // -------------------------------------
    // ie. Red Green Blue Yellow
    //  1:  |    -    -     -
    //  2:  -    |    -     -
    //  1:  -    -    |     -
    //  2:  -    -    -     |
    if ( (ilc > 2 && (ilc % 2) == 0) && BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
    {
      stp = ilc / 2;
    }
    else
    {
      stp = ilc;
    }

    if ( stp <= 2 )
    {
      part = 255;
    }
    else
    {
      part = 255 / stp;
    }

    // If I don't save top, counting upwards has an extra step when
    // compared to downwards counting.
    top = part * stp;
    //F_LOGI(true, true, LC_GREY, "ilc: %d, stp: %d, part: %d, top: %d", ilc, stp, part, top);

    // Initialise our control array
    uint8_t tmode = T_UP;
    for ( uint8_t x = 0; x < stp; x++ )
    {
      interleave[x].brightness = part * x;
      interleave[x].mode = tmode;
      interleave[x].delay = 0;

      // Only invert on binary patterns
      if ( (stp == 2) && mode == T_BOTH )
      {
        tmode = (tmode == T_DOWN)?T_UP:T_DOWN;
      }
      F_LOGV(true, true, LC_GREY, "interleave[%d].brightness: %d, interleave[%d].mode: %d", x, interleave[x].brightness, x, interleave[x].mode);
    }

    // Hysteresis...
    //last_dir = BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
    last_palette = cvars->thisPalette;
  }
  else if ( (prng() % 101) < 5 )
  {
    fs = (prng() % sizeof (aFadeSpd));
    ss = (prng() % sizeof (aStepSpd));
    F_LOGV(true, true, LC_GREY, "aStepSpd[%d] = %d, aFadeSpd[%d] = %d", ss, aStepSpd[ss], fs, aFadeSpd[fs]);
  }
  else
  {
    // Set our pattern delay
    cvars->delay_ticks = aStepSpd[ss];
  }

  // Set the LEDs
  for ( uint8_t x = 0; x < stp; x++ )
  {
    for ( uint16_t i = x; i < count; i += stp )
    {
      setPixelHSV(linear2quadrant(i, start, count), hsvFromPalette(curPalette[cvars->thisPalette], i, interleave[x].brightness), true, cvars->dim);
    }
  }

  // Rotate the cycle
  for ( uint8_t x = 0; x < stp; x++ )
  {
    if ( interleave[x].delay > 0 )
    {
      //F_LOGI(true, true, LC_GREY, "A) interleave[%d].delay: %d", x, interleave[x].delay);
      interleave[x].delay--;
    }
    else if ( interleave[x].mode == T_DOWN )
    {
      //F_LOGI(true, true, LC_GREY, "A) interleave[%d].brightness: %d", x, interleave[x].brightness);
      //interleave[x].brightness -= (T_SPEED / stp);
      interleave[x].brightness -= aFadeSpd[fs];
      //F_LOGI(true, true, LC_GREY, "B) interleave[%d].brightness: %d", x, interleave[x].brightness);

      if ( interleave[x].brightness <= 0 )
      {
        if ( BTST (mode, T_UP) )
        {
          interleave[x].brightness = 0;
          interleave[x].mode = T_UP;
        }
        else
        {
          interleave[x].brightness = top;
        }
        interleave[x].delay = T_DELAY;
        //F_LOGI(true, true, LC_GREY, "BOT: interleave[%d].brightness: %d", x, interleave[x].brightness);
      }
    }
    else
    {
      //F_LOGI(true, true, LC_GREY, "C) interleave[%d].brightness: %d", x, interleave[x].brightness);
      //interleave[x].brightness += (T_SPEED / stp);
      interleave[x].brightness += aFadeSpd[fs];
      //F_LOGI(true, true, LC_GREY, "D) interleave[%d].brightness: %d", x, interleave[x].brightness);

      if ( interleave[x].brightness >= top )
      {
        if ( BTST (mode, T_DOWN) )
        {
          interleave[x].brightness = top;
          interleave[x].mode = T_DOWN;
        }
        else
        {
          interleave[x].brightness = 0;
        }
        interleave[x].delay = T_DELAY;
        //F_LOGI(true, true, LC_GREY, "TOP: interleave[%d].brightness: %d", x, interleave[x].brightness);
      }
    }
  }
  //F_LOGI(true, true, LC_GREY, "Tick");
}

// *********************************************************
// *********************************************************
// HSV Rainbow Cycle
void rainbow_one (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t j = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_RAINBOW;

  rgbFillSolid (start, count, wheel (j), true, cvars->dim);

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    j--;
  }
  else
  {
    j++;
  }
}

// *********************************************************
// *********************************************************
void rainbow_two (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t j = 0;
  uint16_t curPos = start;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_RAINBOW;

  for ( uint16_t i = 0; i < count; i++ )
  {
    setPixelRGB(curPos, wheel ((((curPos * 256) / count) + j) & 255), true, cvars->dim);

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);
  }

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    j--;
  }
  else
  {
    j++;
  }
}

// *********************************************************
// *********************************************************
// fill the dots one after the other with said color
// good for testing purposes
void colorwipe_1 (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t pos = 0;

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  restorePalette (start, count);

  // Set our pattern delay
  cvars->delay_ticks = DELAY_COLORWIPE_ONE;

  cRGB colour = rgbFromPalette (curPalette[cvars->thisPalette], cvars->palIndex);

  // Calculate where our moving 'dot' is...
  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    if ( ++pos >= count )
    {
      pos = 0;
    }
  }
  else
  {
    if ( pos-- == 0 )
    {
      pos = (count - 1);
    }
  }

  // Transfer the linear location of the 'dot' to the possibly non linear
  // location in our LED display
  uint16_t curPos = linear2quadrant (pos, start, count);

  setPixelRGB(curPos, colour, true, cvars->dim);
}

// *********************************************************
// *********************************************************
void colorwipe_2 (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t pos[2] = {};
  static cRGB col[2];
  static uint8_t dir[2];

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  restorePalette (start, count);

  // Set our pattern delay
  cvars->delay_ticks = DELAY_COLORWIPE_ONE;

  // Prepare for loop (TODO: Work on structure to make loops easier)
  col[0] = rgbFromPalette (curPalette[cvars->thisPalette], cvars->palIndex);
  dir[0] = BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);

  col[1] = rgbFromPalette (curPalette[cvars->thatPalette], cvars->palIndex);
  dir[1] = BTST(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT);

  for ( uint8_t x = 0; x < 2; x++ )
  {
    // Calculate where our moving 'dot' is...
    if ( dir[x] == DIR_LEFT )
    {
      if ( ++pos[x] >= count )
      {
        pos[x] = 0;
      }
    }
    else
    {
      if ( pos[x]-- == 0 )
      {
        pos[x] = (count - 1);
      }
    }

    // We don't want to always cut off at the beginning/end, so randomise where we
    // put jumps in position
    if ( (prng() % 400) == 1 )
    {
      pos[x] = (prng() % count);
    }

    // Transfer the linear location of the 'dot' to the possibly non linear
    // location in our LED display
    uint16_t curPos = linear2quadrant (pos[x], start, count);

    //printf("pos: %3d, curPos: %3d, start: %d, count: %d\n", pos, curPos, start, count);

    setPixelRGB(curPos, col[x], true, cvars->dim);
  }
}

// *********************************************************
// * Undulate: https://github.com/phillipa/LEDCode/blob/master/Hyperion/Hyperion.ino
// *********************************************************
void undulate (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t undulate_length = 0;
  uint16_t curPos = start;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_UNDULATE;

  for ( uint16_t i = 0; i < count; i++ )
  {
    setPixelRGB(curPos, rgbFromPalette(curPalette[cvars->thisPalette], undulate_length + (i / cvars->palette_stretch)), true, cvars->dim);

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);
  }

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    undulate_length++;
  }
  else
  {
    undulate_length--;
  }

  if ( (prng() % 1000) == 20 )
  {
    undulate_length = (prng() % 4);
    cvars->palette_stretch = 1 + (prng() % 4);
    //F_LOGI(true, true, LC_GREY, "undulate() delay: %d, undulate_length: %d, palette_stretch: %d", delay, undulate_length, palette_stretch);
  }
}

// *********************************************************
// *********************************************************
void drawLine (int16_t lStart, uint16_t lCount, uint16_t start, uint16_t count, cRGB colour, bool wrap, bool save, uint8_t dim)
{
  uint16_t curPos = lStart;

  for ( uint16_t i = 0; i < lCount; i++ )
  {
    setPixelRGB(curPos, colour, save, dim);

    if ( ++curPos >= control_vars.pixel_count )
    {
      if ( wrap )
      {
        curPos = 0;
      }
      else
      {
        return;
      }
    }
    else if ( curPos >= (start + count) )
    {
      if ( wrap )
      {
        curPos = start;
      }
      else
      {
        return;
      }
    }
  }
}

void candycorn (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t pos = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_CANDYCORN;

  //rgbFillSolid(start, count, CRGBBlack, true, cvars->dim);
  //rgbFillSolid(start, count, CRGBLightYellow, true, cvars->dim);
  rgbFillSolid(start, count, CRGBOrangeRed, true, cvars->dim);
  //rgbFadeAll(start, count, 50);

  // The width of the dashes is a fraction of the dashperiod, with a minimum of one pixel
  uint16_t dashspacing = (count / 8);
  uint16_t dashwidth = 8;

  // Loop through all the LEDs
  for ( int16_t x = ((start - dashspacing) + pos); x < (start + count); x += dashspacing )
  {
    if ( x < start )
    {
      if ( (x + dashwidth) >= 0 )
      {
        drawLine(start, (x + dashwidth), start, count, CRGBOrange, false, true, cvars->dim);
      }
    }
    else
    {
      drawLine(x, dashwidth, start, count, CRGBOrange, false, true, cvars->dim);
    }
    //rgbFillSolid(x + dashwidth, dashwidth, CRGBOrangeRed, true, cvars->dim);
    //rgbFillSolid(x + (dashwidth * 2), dashwidth, CRGBOrange, true, cvars->dim);
  }

  // Cycle
  if ( ++pos >= dashspacing )
  {
    pos = 0;
  }
}

// *********************************************************
// *********************************************************
cRGB bgColors[] = {
  {0x00, 0x00, 0x00},
  {0x00, 0x00, 0xF0},
  {0x00, 0xF0, 0x00},
  {0x00, 0xF0, 0xF0},
  {0xF0, 0x00, 0x00},
  {0xF0, 0x00, 0xF0},
  {0xF0, 0xF0, 0x00}
};

void matrix(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  uint8_t tr = prng() % 90;
  uint8_t currentBgclr = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_MATRIX;

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  //restorePalette();

  if ( tr > 50 )
  {
    if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
    {
      setPixelRGB(linear2quadrant (0, start, count), rgbFromPalette (curPalette[cvars->thisPalette], cvars->palIndex), true, cvars->dim);
    }
    else
    {
      setPixelRGB(linear2quadrant (count - 1, start, count), rgbFromPalette (curPalette[cvars->thisPalette], cvars->palIndex), true, cvars->dim);
    }
  }
  else
  {
    if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
    {
      setPixelRGB(linear2quadrant (0, start, count), bgColors[currentBgclr], true, cvars->dim);
    }
    else
    {
      setPixelRGB(linear2quadrant (count - 1, start, count), bgColors[currentBgclr], true, cvars->dim);
    }
  }

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    for ( int i = count - 1; i > 0; i-- )
      setPixelRGB(linear2quadrant (i, start, count), getPixelRGB (linear2quadrant (i - 1, start, count)), true, 0);
  }
  else
  {
    for ( uint16_t i = 0; i < count - 1; i++ )
      setPixelRGB(linear2quadrant (i, start, count), getPixelRGB (linear2quadrant (i + 1, start, count)), true, 0);
  }
}

// *********************************************************
// * One sin
// *********************************************************
uint8_t thishue = 80;
uint8_t thathue = 140;
uint8_t thisrot = 5;
uint8_t thatrot = 1;
uint8_t allfreq = 2;
uint8_t allsat = 255;
uint8_t thiscutoff = 200;
uint8_t thatcutoff = 200;
uint8_t thisBPM = 5;
uint8_t colorindex = 0;
uint8_t thisstep = 15;
uint8_t use_qsub = false;
int8_t thatspeed = 3;
int8_t thisspeed = 3;
long thisphase = 0;
long thatphase = 0;

// * Two sin
void two_col_init (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  thisstep = 10;      //
  allfreq = 10;      // >= is shorter lengths
  thisspeed = 10;      // >= is faster
  thiscutoff = 100;     // >= shorter chain when using qsub
  cvars->delay_ticks = DELAY_ONESIN;
  one_sin (start, count, step, cvars, false);
}

//
void one_sin_init (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static bool onecol = true;
  if ( (prng() % 1000) == 1 )
  {
    // toggle one colour
    onecol = (1 - onecol);
  }
  thisstep = 40;
  allfreq = 4;
  thisspeed = 5;
  thiscutoff = 200;     // >= shorter chain when using qsub
  cvars->delay_ticks = DELAY_ONESIN;

  one_sin (start, count, step, cvars, onecol);
}

uint8_t next_colour (uint8_t cur_col, bool direction)
{
  uint8_t retval = cur_col;

  if ( direction == DIR_LEFT )
  {
    if ( ++retval > 15 )
    {
      retval = 0;
    }
  }
  else if ( retval == 0 )
  {
    retval = 15;
  }
  else
  {
    --retval;
  }

  return retval;
}

void one_sin (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars, bool oneColour)
{
  static uint16_t x = 0;
  static uint16_t y = 0;
  static uint8_t start_col = 0;
  static int  start_bri = 0;
  uint16_t save_x = x;
  uint16_t save_y = y;
  cHSV pixel;

  uint8_t cur_col = start_col;
  int lastbright = start_bri;
  int thisbright = 0;

  // Direction
  bool cur_dir = BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
  if ( cur_dir == DIR_LEFT )
  {
    thisphase += thisspeed;
  }
  else
  {
    thisphase -= thisspeed;
  }

  // Random type
  // ==============================================================
  if ( (use_qsub) && (prng() % 5000) == 1 )
  {
    use_qsub = false;
  }
  else if ( (prng() % 10000) == 1 )
  {
    use_qsub = true;
  }

  // Loop through all LEDs
  // ==============================================================
  for ( uint16_t k = 0; k < count; k++ )
  {
    if ( use_qsub )
    {
      thisbright = qsubd(cubicwave8 ((k * allfreq) + thisphase), thiscutoff);
    }
    else
    {
      thisbright = cubicwave8((k * allfreq) + thisphase);
    }

    // Slightly over complicated tracking of colour boundaries
    // ==============================================================
    if ( k == 0 )
    {
      if ( lastbright == 0 && (thisbright != lastbright) )
      {
        // DIR_LEFT
        if ( cur_dir == DIR_LEFT )
        {
          cur_col = next_colour(cur_col, cur_dir);
        }
      }
      else if ( thisbright == 0 && (thisbright != lastbright) )
      {
        // DIR_RIGHT
        if ( cur_dir == DIR_RIGHT )
        {
          cur_col = next_colour(cur_col, cur_dir);
        }
      }

      // Save first calculation for use next run
      // ==============================================================
      start_col = cur_col;
      start_bri = thisbright;
    }
    else if ( lastbright == 0 && (thisbright != lastbright) )
    {
      cur_col = next_colour(cur_col, DIR_LEFT);
    }

    // Save this brightness level for later comparison
    lastbright = thisbright;

    if ( oneColour )
    {
      pixel = hsvFromPalette(curPalette[cvars->thisPalette], cur_col, thisbright);
    }
    else
    {
      pixel = hsvFromPalette(curPalette[cvars->thisPalette], (k + y), thisbright);
    }

    // Display the pixel
    setPixelHSV(linear2quadrant(k, start, count), pixel, true, cvars->dim);
  }

  x = save_x + 0;
  y = save_y + 0;
}

// *********************************************************
// * Two sin
// *********************************************************
void two_sin_init(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_TWOSIN;

  two_sin(start, count, step, cvars, rgb2hsv(cvars->this_col), rgb2hsv(cvars->that_col));
}

void two_sin(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars, cHSV cola, cHSV colb)
{
  static uint8_t x = 0;
  static uint8_t y = 0;
  cHSV pixela;
  cHSV pixelb;

  thisstep = 4;
  thisrot = 1;
  allfreq = 8;
  thisspeed = 4;
  thatspeed = 4;
  thiscutoff = 128;                                                  // Cool (ish)

  // One colour?
  if ( cola.v > 0 )
  {
    pixela = cola;
  }
  if ( colb.v > 0 )
  {
    pixelb = colb;
  }

  // Direction...
  bool cur_dir = BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT);
  if ( cur_dir == DIR_LEFT )
  {
    thisphase += thisspeed;
  }
  else
  {
    thisphase -= thisspeed;
  }
  if ( BTST(control_vars.bitflags, DISP_BF_THAT_DIR_LEFT) )
  {
    thatphase += thatspeed;
  }
  else
  {
    thatphase -= thatspeed;
  }

  thishue += thisrot;
  thathue += thatrot;

  // Loop through all LEDs
  uint16_t curPos = start;
  for ( uint16_t k = 0; k < count; k++ )
  {
    int thisbright = qsubd(cubicwave8((k * allfreq) + thisphase), thiscutoff);
    int thatbright = qsubd(cubicwave8((k * allfreq) + 128 + thatphase), thatcutoff);

    if ( cola.v > 0 )
    {
      pixela.v = thisbright;
    }
    else
    {
      pixela = hsvFromPalette(curPalette[cvars->thisPalette], k + y, thisbright);
    }

    if ( colb.v > 0 )
    {
      pixelb.v = thatbright;
    }
    else
    {
      pixelb = hsvFromPalette(curPalette[cvars->thatPalette], k + y, thatbright);
    }

    cRGB x = hsv2rgb(pixela);
    cRGB y = hsv2rgb(pixelb);
    //setPixelRGB(curPos, blendRGB(x, y), true, cvars->dim);
    setPixelRGB(curPos, nblend(x, y, 127), true, cvars->dim);

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);
  }

  // Are we shifting the palette?
  if ( thisstep )
  {
    if ( ++x > thisstep )
    {
      x = 0;
      y++;
    }
  }
}

// *********************************************************
// * TwinkleMapPixels
// *********************************************************
#define MAX_BRIGHTNESS      255
#define MIN_BRIGHTNESS      0
#define INI_BRIGHTNESS      0                                        // Has to be > 0 or everything is white :)
#define MIN_SPEED           1                                        //
#define MAX_SPEED           4                                        //
#define CHANCE_OF_FLASH     50
typedef enum
{
  Init, Off, GettingBrighter, GettingDimmer, Star
} twinkleState;
typedef struct
{
  twinkleState state;
  uint8_t brightness;
  uint8_t speed;
} twinkle;
void twinklemappixels(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static twinkle PixelState[PIXELS];

  // Set our pattern delay
  cvars->delay_ticks = DELAY_TWINKLEMAP;

  uint16_t curPos = start;
  for ( uint16_t i = 0; i < count; i++ )
  {
    cHSV pixel = hsvFromPalette(curPalette[cvars->thisPalette], curPos, INI_BRIGHTNESS);

    if ( PixelState[curPos].state == Init )
    {
      PixelState[curPos].state = GettingBrighter;
      PixelState[curPos].speed = (prng() % MAX_SPEED) + MIN_SPEED;
    }
    else if ( PixelState[curPos].state == Star )
    {
      // If set, we are definitely greater than zero
      if ( BTST(--PixelState[curPos].speed, 1) )
      {
          pixel.v = 255;
      }
      else
      {
        // We don't want the LED on
        pixel.v = 0;

        if ( PixelState[curPos].speed == 0 )
        {
          PixelState[curPos].state = GettingBrighter;
          PixelState[curPos].speed = (prng() % MAX_SPEED) + MIN_SPEED;
        }
      }
    }
    else
    {
      if ( PixelState[curPos].state == GettingBrighter )
      {
        if ( int(PixelState[curPos].brightness + PixelState[curPos].speed) >= MAX_BRIGHTNESS )
        {
          PixelState[curPos].brightness = MAX_BRIGHTNESS;
          PixelState[curPos].state      = GettingDimmer;
        }
        else
        {
          PixelState[curPos].brightness += PixelState[curPos].speed;
        }
      }
      else if ( PixelState[curPos].state == GettingDimmer )
      {
        if ( int(PixelState[curPos].brightness - PixelState[curPos].speed) <= MIN_BRIGHTNESS )
        {
          if ( (prng() % 100) < CHANCE_OF_FLASH )
          {
            pixel.s = 1;                                                // For a white flash
            PixelState[curPos].brightness = MAX_BRIGHTNESS;
            PixelState[curPos].state      = Star;
            PixelState[curPos].speed      = 4;                          // Keep even
          }
          else
          {
            PixelState[curPos].brightness = MIN_BRIGHTNESS;
            PixelState[curPos].state      = GettingBrighter;

            if ( PixelState[curPos].speed > 1 )
            {
              PixelState[curPos].speed -= 1;
            }
            else
            {
              PixelState[curPos].speed = (prng() % MAX_SPEED) + MIN_SPEED;
            }
          }
        }
        else
        {
          PixelState[curPos].brightness -= PixelState[curPos].speed;
        }
      }
      else
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "We shouldn't be here");
      }

      // Flash when the LED is at its minimum brightness
      // -------------------------------------------------
      //pixel.v = sin8(PixelState[curPos].brightness);

      // Flash when the LED is at its maximum brightness
      // -------------------------------------------------
      pixel.v = cos8(PixelState[curPos].brightness);
    }

    setPixelHSV(curPos, pixel, true, cvars->dim);

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);
  }
}

// *********************************************************
// * Pretty little stars all over the place
// *********************************************************
bool dazzle (controlvars_t *cvars, uint8_t stars_count, dazzle_mode mode)
{
  static char initialised = 0;
  static starStruc stars[MAX_STARS];
  unsigned int c;
  bool update = false;

  // Initialise the stars on first pass
  if ( !initialised )
  {
    for ( c = 0; c < MAX_STARS; c++ )
    {
      stars[c].pixel = 0;
      stars[c].on = false;
    }
    initialised = 1;
  }
  // Check stars_count is within range or we ignore displaying sparklies
  else if ( stars_count <= MAX_STARS )
  {
    // Iterate through the number of stars and process each one
    for ( c = 0; c < stars_count; c++ )
    {
      if ( stars[c].on )
      {
        // FIXME: --stars[c].counter duplication needs rework
        switch ( mode )
        {
          case GLITTER:
            if ( --stars[c].counter == 0 )
            {
              stars[c].on = false;
            }
            break;
          case CONFETTI:
            if ( (int16_t)(stars[c].colour.v - stars[c].speed) < MIN_BRIGHTNESS )
            {
              if ( --stars[c].counter == 0 )
              {
                stars[c].on = false;
              }
            }
            else
            {
              stars[c].colour.v -= stars[c].speed;
            }
            break;
          case FLUTTER:
            if ( stars[c].colour.v > 0 )
            {
              if ( --stars[c].counter == 0 )
              {
                stars[c].on = false;
              }
              else
              {
                stars[c].colour.v = 0;
              }
            }
            else
            {
              /*
              if ( --stars[c].counter == 0 )
              {
                stars[c].on = false;
              }
              else
              */
              {
                stars[c].colour.v = 255;
              }
            }
            break;
          case SNOW:
            break;
        }

        if ( stars[c].on == true )
        {
          setPixelHSV(stars[c].pixel, stars[c].colour, false, star_bright[cvars->dim]);
        }
        else
        {
          // Restore the original pixel
          setPixelRGB(stars[c].pixel, getPixelRGB(stars[c].pixel), true, 0);
        }
      }
      else if ( (prng() % 100) < 20 )
      {
        stars[c].pixel = (prng() % cvars->pixel_count);
        stars[c].colour = rgb2hsv (CRGBWhite);
        stars[c].on = true;
        stars[c].counter = 4;
        stars[c].speed = (prng() % 4) + 2;
        setPixelHSV(stars[c].pixel, stars[c].colour, false, star_bright[cvars->dim]);
      }

      // Do we need to update the display?
      if ( stars[c].on == true )
      {
        update = true;
      }
    }
  }

  return update;
}

// *********************************************************
// * This is adapted from a routine created by Mark Kriegsman
// *********************************************************
void juggle(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  cHSV pixela;
  cHSV pixelb;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_JUGGLE;

  // outBuffer was cleared, so we need to restore it from the rgbBuffer
  restorePalette(start, count);

  // Fade before we run the code
  rgbFadeAll(start, count, 5);

  uint16_t curPos = start;
  for ( uint16_t i = 0; i < 40; i++ )
  {
    int x = beatsin16(i + 60, 0, count, 0, 65535);
    curPos += x;
    if ( curPos >= (start + count) )
    {
      curPos = curPos - (start + count);
    }

    pixela = getPixelHSV(curPos);
    pixelb = hsvFromPalette(curPalette[cvars->thisPalette], curPos, 220);

    //setPixelHSV(x, blendHSV(pixela, pixelb, 0.5), true);
    setPixelRGB(curPos, blendRGB(hsv2rgb(pixela), hsv2rgb (pixelb)), true, cvars->dim);

    curPos = inc_curpos(curPos, start, count);
  }
}

// *********************************************************
// * Palette scroller
// *********************************************************
void palettescroll(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static int16_t ss = -1;
  static int16_t si = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_PALETTESCROLL;

  if ( ss != start )
  {
    ss = start;
    si = start;
  }
  else
  {
    if ( si < ss )
    {
      si = (ss + count);
    }
    else if ( si >= (ss + count) )
    {
      si = ss;
    }
  }

  FillLEDsFromPaletteColors(si, start, count);

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    si--;
  }
  else
  {
    si++;
  }
}

// *********************************************************
// * Strobe
// *********************************************************
#define LOOP_ITEM     0
#define LOOP_BEGIN    1
#define LOOP_END      2
#define ARRAY_END     3
typedef struct
{
  int16_t       cmd;     /* Loop command (Begin/Item/End) */
  int16_t       ite;     /* Loop iterations */
  int16_t       cyc;     /* # Clock Cycles to display colour for */
  cRGB          color;
} strobe_data_t;
typedef struct
{
  int16_t       lsp;     // Loop Start Position
  int16_t       cic;     // Current Iteration Count
  int16_t       ccc;     // Current Cycle Count
  int16_t       cai;     // Current Array Index (Our position in the data list)
  strobe_data_t *data;
} strobe_item_t;
void strobe(uint16_t start, uint16_t count, uint16_t width, uint16_t gap, strobe_item_t *strobe_item, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = 0;

  // ******************************************************
  // Check we have pattern data
  // ******************************************************
  strobe_data_t *data = strobe_item->data;
  if ( !data )
  {
    return;
  }

  // ******************************************************
  // Process display; Set the desired colour and spacing
  // ******************************************************
  int16_t curPos = start;
  int16_t curWidth = width;
  for ( uint16_t i = 0; i < count; i++ )
  {
    if ( curWidth )
    {
      setPixelRGB(curPos, data[strobe_item->cai].color, true, cvars->dim);
      if ( --curWidth == 0 )
      {
        curWidth = width;
        curPos += gap;
        i += gap;
      }
    }
    curPos++;

    // Increment our pixel position and check for overflow
    // ******************************************************
    curPos = constrain_curpos(curPos, start, count);
  }

  // Check if this is the start of a loop and process
  // ******************************************************
  if ( (strobe_item->cic == 0) && data[strobe_item->cai].cmd == LOOP_BEGIN )
  {
    strobe_item->lsp = strobe_item->cai;
  }

  // ******************************************************
  // Increment and check if we have displayed this colour
  // the requested number of display cycles.
  // ******************************************************
  if ( ++strobe_item->ccc >= data[strobe_item->cai].cyc )
  {
    // Reset our cycle count
    // ******************************************************
    strobe_item->ccc = 0;

    // Firstly, check if we are at loop end...
    // ******************************************************
    if ( data[strobe_item->cai].cmd == LOOP_END )
    {
      // Check if we have completed all requested iterations
      // ******************************************************
      if ( ++strobe_item->cic >= data[strobe_item->lsp].ite )
      {
        strobe_item->cic = 0;         // Reset iteration count
        strobe_item->cai++;           // Increment out index position
      }
      else
      {
        strobe_item->cai = strobe_item->lsp;
      }
    }
    // Check if we have reached the last line of our array
    // ******************************************************
    else if ( data[strobe_item->cai].cmd == ARRAY_END )
    {
      strobe_item->cic = 0;
      strobe_item->cai = 0;
    }
    // Or just move onto the next line...
    // ******************************************************
    else
    {
      strobe_item->cai++;
    }
  }
}
strobe_data_t police_a[] = {
//  command    | loops | cycles | colour
  {LOOP_BEGIN,     8,      4,     Red},
  {LOOP_END,       0,      2,     Black},

  {LOOP_ITEM,      0,      48,    Black},

  {LOOP_BEGIN,     8,      4,     Red},
  {LOOP_END,       0,      2,     Black},

  {LOOP_ITEM,      0,      48,    Black},

  {ARRAY_END,      0,      50,    Black}
};
strobe_data_t police_b[] = {
//  command    | loops | cycles | colour
  {LOOP_ITEM,      0,      48,    Black},

  {LOOP_BEGIN,     8,      4,     Blue},
  {LOOP_END,       0,      2,     Black},

  {LOOP_ITEM,      0,      48,    Black},

  {LOOP_BEGIN,     8,      4,     Blue},
  {LOOP_END,       0,      2,     Black},

  {ARRAY_END,      0,      50,    Black}
};
// *********************************************
// * Pseudo Police lights
// *********************************************
void police_lights(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static strobe_item_t *police_item_a = NULL;
  static strobe_item_t *police_item_b = NULL;
  if ( police_item_a == NULL )
  {
    F_LOGI(true, true, LC_GREY, "Initialising data...");
    police_item_a = (strobe_item_t *) pvPortMalloc (sizeof (strobe_item_t));
    memset(police_item_a, 0, sizeof(strobe_item_t));
    police_item_b = (strobe_item_t *) pvPortMalloc(sizeof(strobe_item_t));
    memset(police_item_b, 0, sizeof(strobe_item_t));
    police_item_a->data = police_a;
    police_item_b->data = police_b;
  }

  strobe(start, count, 4, 4, police_item_a, cvars);
  strobe(start+4, count, 4, 4, police_item_b, cvars);
}

// *********************************************************
// * Discostrobe: https://gist.github.com/kriegsman/626dca2f9d2189bd82ca
// *********************************************************
#define ZOOMING_BEATS_PER_MINUTE 11
void discostrobe(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_DISCOSTROBE;

  // First, we black out all the LEDs
  rgbFillSolid(start, count, CRGBBlack, true, cvars->dim);

  // To achive the strobe effect, we actually only draw lit pixels
  // every Nth frame (e.g. every 4th frame).--
  // sStrobePhase is a counter that runs from zero to kStrobeCycleLength-1,
  // and then resets to zero.--
  const uint8_t kStrobeCycleLength = 10;  // light every Nth frame
  static uint8_t sStrobePhase = 0;
  sStrobePhase = sStrobePhase + 1;
  if ( sStrobePhase >= kStrobeCycleLength )
  {
    sStrobePhase = 0;
  }

  // We only draw lit pixels when we're in strobe phase zero;-
  // in all the other phases we leave the LEDs all black.
  if ( sStrobePhase == 0 )
  {
    // The dash spacing cycles from 4 to 9 and back, 8x/min (about every 7.5 sec)
    uint8_t dashperiod = beatsin8 (8 /*cycles per minute */, 4, 10, 0, 255);
    // The width of the dashes is a fraction of the dashperiod, with a minimum of one pixel
    uint8_t dashwidth = (dashperiod / 4) + 1;

    // The distance that the dashes move each cycles varies-
    // between 1 pixel/cycle and half-the-dashperiod/cycle.
    // At the maximum speed, it's impossible to visually distinguish
    // whether the dashes are moving left or right, and the code takes
    // advantage of that moment to reverse the direction of the dashes.
    // So it looks like they're speeding up faster and faster to the
    // right, and then they start slowing down, but as they do it becomes
    // visible that they're no longer moving right; they've been-
    // moving left.  Easier to see than t o explain.
    //
    // The dashes zoom back and forth at a speed that 'goes well' with
    // most dance music, a little faster than 120 Beats Per Minute.  You
    // can adjust this for faster or slower 'zooming' back and forth.
    uint8_t zoomBPM = ZOOMING_BEATS_PER_MINUTE;
    int8_t dashmotionspeed = beatsin8((zoomBPM / 2), 1, dashperiod, 0, 255);
    // This is where we reverse the direction under cover of high speed
    // visual aliasing.
    if ( dashmotionspeed >= (dashperiod / 2) )
    {
      dashmotionspeed = 0 - (dashperiod - dashmotionspeed);
    }

    // The hueShift controls how much the hue of each dash varies from-
    // the adjacent dash.  If hueShift is zero, all the dashes are the-
    // same color. If hueShift is 128, alterating dashes will be two
    // different colors.  And if hueShift is range of 10..40, the
    // dashes will make rainbows.
    // Initially, I just had hueShift cycle from 0..130 using beatsin8.
    // It looked great with very low values, and with high values, but
    // a bit 'busy' in the middle, which I didnt like.
    //   uint8_t hueShift = beatsin8(2,0,130);
    //
    // So instead I layered in a bunch of 'cubic easings'
    // (see http://easings.net/#easeInOutCubic)
    // so that the resultant wave cycle spends a great deal of time
    // "at the bottom" (solid color dashes), and at the top ("two
    // color stripes"), and makes quick transitions between them.
    uint8_t cycle = beat8 (2, 0);  // two cycles per minute
    uint8_t easedcycle = ease8InOutCubic(ease8InOutCubic(cycle));
    uint8_t wavecycle = cubicwave8(easedcycle);
    uint8_t hueShift = scale8(wavecycle, 130);

    // Each frame of the animation can be repeated multiple times.
    // This slows down the apparent motion, and gives a more static
    // strobe effect.  After experimentation, I set the default to 1.
    uint8_t strobesPerPosition = 1; // try 1..4

    // Now that all the parameters for this frame are calculated,
    // we call the 'worker' function that does the next part of the work.
    discoworker (cvars, dashperiod, dashwidth, dashmotionspeed, strobesPerPosition, hueShift);
  }
}

// discoWorker updates the positions of the dashes, and calls the draw function
//
void discoworker (controlvars_t *cvars, uint8_t dashperiod, uint8_t dashwidth, int8_t dashmotionspeed, uint8_t stroberepeats, uint8_t huedelta)
{
  static int16_t sStartPosition = 0;
  static uint8_t sRepeatCounter = 0;
  static uint8_t sStartHue = 0;

  // Always keep the hue shifting a little
  sStartHue += 1;

  // Increment the strobe repeat counter, and
  // move the dash starting position when needed.
  sRepeatCounter = sRepeatCounter + 1;
  if ( sRepeatCounter >= stroberepeats )
  {
    sRepeatCounter = 0;

    sStartPosition = sStartPosition + dashmotionspeed;

    // These adjustments take care of making sure that the
    // starting hue is adjusted to keep the apparent color of
    // each dash the same, even when the state position wraps around.
    if ( sStartPosition >= dashperiod )
    {
      while ( sStartPosition >= dashperiod )
      {
        sStartPosition -= dashperiod;
      }
      sStartHue -= huedelta;
    }
    else if ( sStartPosition < 0 )
    {
      while ( sStartPosition < 0 )
      {
        sStartPosition += dashperiod;
      }
      sStartHue += huedelta;
    }
  }

  // draw dashes with full brightness (value), and somewhat
  // desaturated (whitened) so that the LEDs actually throw more light.
  const uint8_t kSaturation = 208;
  const uint8_t kValue = 255;

  // call the function that actually just draws the dashes now
  drawRainbowDashes (cvars, sStartPosition, cvars->pixel_count - 1, dashperiod, dashwidth, sStartHue, huedelta, kSaturation, kValue);
}

// drawRainbowDashes - draw rainbow-colored 'dashes' of light along the led strip:
//   starting from 'startpos', up to and including 'lastpos'
//   with a given 'period' and 'width'
//   starting from a given hue, which changes for each successive dash by a 'huedelta'
//   at a given saturation and value.
//
//   period = 5, width = 2 would be  _ _ _ X X _ _ _ Y Y _ _ _ Z Z _ _ _ A A _ _ _
//                                   \-------/       \-/
//                                   period 5      width 2
//
void drawRainbowDashes (controlvars_t *cvars, uint16_t startpos, uint16_t lastpos, uint8_t period, uint8_t width, uint8_t huestart, uint8_t huedelta, uint8_t saturation, uint8_t value)
{
  uint8_t hue = huestart;
  for ( uint16_t i = startpos; i <= lastpos; i += period )
  {
    // Switched from HSV color wheel to color palette
    // Was: cRGB color = cHSV(hue, saturation, value);
    //cRGB color = rgbFromPalette(curPalette[thisPalette], hue, value);
    cRGB color = rgbFromPalette(curPalette[cvars->thisPalette], hue);

    // draw one dash
    uint16_t pos = i;
    for ( uint8_t w = 0; w < width; w++ )
    {
      setPixelRGB(pos, color, true, cvars->dim);
      pos++;
      if ( pos >= cvars->pixel_count )
      {
        break;
      }
    }

    hue += huedelta;
  }
}

// Arduino map function
long map (long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//######################################### LoneWolf #########################################
typedef struct _lonewolf_
{
  bool on;
  bool blinking;
  bool decrement;
  bool initialised;

  uint8_t speed;
  uint8_t timer;

  uint16_t start;
  uint16_t end;
  uint16_t tail;
  uint16_t current;

  cRGB colour;
} lonewolf;

#ifdef DEBUG_BUILD
#define NUM_WOLVES 10
#else
#define NUM_WOLVES 10
#endif

void loneWolf (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static lonewolf wolves[NUM_WOLVES];
  static uint8_t bgc = 0;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_LONEWOLF;

  // Some randome background colour
  //rgbFillSolid(0, PIXELS, basicColors[bgc], true, cvars->dim);
  rgbFillSolid(start, count, basicColors[bgc], true, 5);
  if ( prng() % 10000 < 50 )
  {
    bgc = prng() & 0xF;
  }

  // Fade before we run the code
  //rgbFadeAll(9);

  for ( uint8_t w = 0; w < NUM_WOLVES; w++ )
  {
    // Initialisation
    // *****************************
    if ( !wolves[w].initialised )
    {
      wolves[w].on = true;
      wolves[w].blinking = false;
      wolves[w].initialised = true;
      wolves[w].current = prng() % count;
      wolves[w].colour = rgbFromPalette(curPalette[cvars->thisPalette], prng() & 0xF);
      wolves[w].timer = (prng() % NUM_WOLVES) + 1;
    }

    // Blinken (no idea)
    // *****************************
    if ( wolves[w].blinking )
    {
      if ( wolves[w].timer > 0 )
        wolves[w].timer--;

      if ( !wolves[w].timer )
      {
        wolves[w].timer = (prng() % 10) + 5;
        BCHG (wolves[w].on, 1);
      }
    }

    // Are we already active?
    // *****************************
    if ( wolves[w].speed )
    {
      if ( wolves[w].timer > 0 )
        wolves[w].timer--;

      if ( !wolves[w].timer )
      {
        wolves[w].timer = wolves[w].speed;

        if ( wolves[w].current != wolves[w].end )
        {
          wolves[w].current = next_linear_pixel(wolves[w].current, count, wolves[w].decrement);
        }
        else if ( wolves[w].tail > 0 )
        {
          // FIXME
          wolves[w].tail--;
        }
        else
        {
          // FIXME
          wolves[w].speed = 0;
          //wolves[w].blinking    = true;
          wolves[w].timer = (prng() % NUM_WOLVES) + 20;
        }
      }

      uint16_t l_c = wolves[w].current;
      for ( uint16_t x = 0; x < wolves[w].tail; x++ )
      {
        if ( l_c == wolves[w].start )
          break;

        // Stop overflow when 'count' is altered
        if ( l_c > count )
        {
          wolves[w].initialised = false;
          break;
        }

        uint16_t q_p = linear2quadrant(l_c, start, count);
        //printf("%3d: l_c: %3d, q_p: %3d, start: %3d, count: %3d\n", x, l_c, q_p, start, count);
        setPixelRGB(q_p, blendRGB(wolves[w].colour, getPixelRGB(q_p)), true, cvars->dim);

        if ( l_c > count )
        {
          F_LOGE(true, true, LC_YELLOW, "Why am I here?");
        }
        l_c = next_linear_pixel(l_c, count, !wolves[w].decrement);
      }
    }
    // Else, throw a die to run
    // *****************************
    else if ( --wolves[w].timer == 0 )
    {
      wolves[w].blinking = false;
      wolves[w].speed = (prng() % 2) + 1;
      wolves[w].colour = rgbFromPalette(curPalette[cvars->thisPalette], prng() & 0xF);

      // Save our start position
      wolves[w].start = prng() % count;
      wolves[w].current = wolves[w].start;

      // Get our next position making sure it is not our own
      do
      {
        wolves[w].end = prng() % count;
      }
      while ( wolves[w].end == wolves[w].start );

      // Set our direction (increment/decrement)
      if ( wolves[w].end > wolves[w].start )
      {
        wolves[w].decrement = false;
      }
      else
      {
        wolves[w].decrement = true;
      }

      // If difference is greater than 50%, we invert the direction
      // (and add a tail)
      // FIXME
      if ( abs (wolves[w].end - wolves[w].start) > (count / 2) )
      {
        wolves[w].decrement = !wolves[w].decrement;
        wolves[w].tail = prng() % (count - (abs (wolves[w].end - wolves[w].start)));
      }
      else
      {
        // FIXME
        wolves[w].tail = prng() % (abs (wolves[w].end - wolves[w].start));
      }

      F_LOGV(true, true, LC_BRIGHT_GREEN, "start: %d, end: %d, decrement: %d, tail: %d", wolves[w].start, wolves[w].end, wolves[w].decrement, wolves[w].tail);
    }
  }
}

#define MAX_INT_VALUE 65536
uint16_t animateSpeed = 270;

//#########################################DoubleChaser#########################################
void doubleChaser(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)  //2 chaser animations offset 180 degrees
{
  static uint16_t animationFrame = 0;
  uint16_t frame = animationFrame * 2;

  // Set our pattern delay
  cvars->delay_ticks = DELAY_DOUBLECHASER;

  rgbFadeAll(start, count, 10);

  Ring(start, count, frame, rgb2hsv(cvars->this_col).h, cvars->dim);
  Ring(start, count, frame + (MAX_INT_VALUE / 2), rgb2hsv(cvars->that_col).h, cvars->dim);

  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    animationFrame += animateSpeed;
  }
  else
  {
    animationFrame -= animateSpeed;
  }
}

//***********************  larsenScanner  *************************
//*****************************************************************
void larsenScanner (uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t animationFrame = 0;
  static uint8_t last_palette = 255;
  static uint8_t width;
  static uint8_t ilc;
  uint16_t pos16;

  // On our first run or when our palette has changed we check
  // if, and what, pattern this palette is made up of.
  if ( last_palette != cvars->thisPalette )
  {
    // Get the pattern interleave (if there is one)
    get_interleave(&width, &ilc, curPalette[cvars->thisPalette]);

    // Save for next iteration
    last_palette = cvars->thisPalette;
  }

  // Set our pattern delay
  cvars->delay_ticks = DELAY_LARSENSCANNER;

  cRGB colour = cvars->this_col;

  // If foreground colour is the same as the background,
  // force the foreground colour to black
  if ( compareColours(cvars->this_col, cvars->that_col) == true )
  {
    set_background(start, count, CRGBBlack, cvars);
  }
  else
  {
    set_background(start, count, cvars->that_col, cvars);
  }

  if ( animationFrame < (MAX_INT_VALUE / 2) )
  {
    pos16 = animationFrame * 2;
  }
  else
  {
    pos16 = MAX_INT_VALUE - ((animationFrame - (MAX_INT_VALUE / 2)) * 2);
  }

  int position = map (pos16, 0, MAX_INT_VALUE, 0, (count * 16));
  drawFractionalBar(position, (count / 5), rgb2hsv(colour).h, 1, start, count, cvars->dim);
  animationFrame += animateSpeed;
}

//********************   Ring   ***********************
// Anti-aliased cyclical chaser, 3 pixels wide
// Color is determined by "hue"
//*****************************************************
void Ring (uint16_t start, uint16_t count, uint16_t animationFrame, uint8_t hue, uint8_t dim)
{
  uint16_t stripLength = count;

  int pos16 = map (animationFrame, 0, MAX_INT_VALUE, 0, ((stripLength) * 16));

  drawFractionalBar(pos16, (count / 5), hue, 1, start, count, dim);
}

//************************          drawFractionalBar           ******************************
// Notes: Fails on black
void drawFractionalBar(int pos16, int width, uint8_t hue, bool wrap, uint16_t start, uint16_t count, uint8_t dim)
{
  uint16_t i = pos16 / 16;            // convert from pos to raw pixel number

#if defined (FAST_HSV)
  cHSV pixel = {hue, 255, 0};         // Our HSV colour
#else
  cHSV pixel = {(float)hue, 1, 0};   // Our HSV colour
#endif
  pixel.v = 0;                        // Min Brightness
  uint16_t margin = width / 2;        // How many LEDs to 'wrap around'
  uint8_t x = 255 / margin;           // Brightness increment

  // Set up our starting position
  int16_t curPos = (start + i) - margin;
  curPos = constrain_curpos(curPos, start, count);

  // Loop through our bar pixels
  for ( int16_t c = (i - margin); c < (i + margin); c++ )
  {
    setPixelHSV(curPos, pixel, true, dim);

    // Increment our pixel position and check for overflow
    curPos = inc_curpos(curPos, start, count);

    // Check if we are getting dimmer or brighter
    if ( c < i )
    {
      pixel.v += x;
    }
    else
    {
      pixel.v -= x;
    }
  }
}

#if defined (FAST_HSV)
uint8_t constrain_hue (enum hue_color color, uint8_t h)
{
  float rh = h;

  if ( h > hues[color].hue_max )
  {
    rh = hues[color].hue_min;
  }
  else if ( h < hues[color].hue_min )
  {
    rh += hues[color].hue_max;
  }

  return rh;
}
#else //FAST_HSV
float constrain_hue (enum hue_color color, float h)
{
  float rh = h;

  if ( h > hues[color].hue_max )
  {
    rh = hues[color].hue_min;
  }
  else if ( h < hues[color].hue_min )
  {
    rh += hues[color].hue_max;
  }

  return rh;
}
#endif  // FAST_HSV
#define spacing 5
void color_wave(enum hue_color color, uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
#if defined (FAST_HSV)
  static uint8_t h;
  uint8_t hTemp;
#else   // FAST_HSV
  static float h;
  float hTemp;
#endif  // FAST_HSV
  static _hue_color_t last_color = HUE_MAX;

  if ( last_color != color )
  {
    last_color = color;
    h = hues[color].hue_min;
  }
  h += .2 * spacing;
  h = constrain_hue(color, h);
  hTemp = h;

  //printf("hue: %d\n", hTemp);

  uint16_t curPos = start;
  for ( uint16_t i = start; i < count; i++ )
  {
#if defined (FAST_HSV)
    cRGB pixel = hsv2rgb({hTemp, 255, 255});
#else
    cRGB pixel = hsv2rgb({hTemp, 1, 255});
#endif
    pixel.r = 0xff;
    pixel.g = 0x0;
    //pixel.b = 0x0;
    setPixelRGB(curPos, pixel, true, cvars->dim);
    hTemp += .2 * spacing;
    hTemp = constrain_hue(color, hTemp);

    curPos = inc_curpos(curPos, start, count);
  }
}
void color_waves(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_COLORWAVES;

  color_wave(HUE_ALL, start, count, step, cvars);
}

void side_rain(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Set our pattern delay
  cvars->delay_ticks = DELAY_SIDERAIN;

  logicalShift(start, count, BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT), true);

  uint8_t x = RANDOM8_MIN_MAX(0, 4);
  if ( BTST(control_vars.bitflags, DISP_BF_THIS_DIR_LEFT) )
  {
    if ( x == 1 )
#if defined (FAST_HSV)
      // FIXME: Check cycle_hue size
      setPixelHSV((start + count) - 1, (cHSV) {cvars->cycle_hue, 255, 255}, true, cvars->dim);
#else
      setPixelHSV((start + count) - 1, (cHSV) {(float)cvars->cycle_hue, 1, 255}, true, cvars->dim);
#endif
    else if ( x != 2 )
      setPixelRGB((start + count) - 1, CRGBBlack, true, 0);
  }
  else
  {
    if ( x == 1 )
#if defined (FAST_HSV)
      // FIXME: Check cycle_hue size
      setPixelHSV((start + count) - 1, (cHSV) {cvars->cycle_hue, 255, 255}, true, cvars->dim);
#else
      setPixelHSV((start + count) - 1, (cHSV) {(float)cvars->cycle_hue, 1, 255}, true, cvars->dim);
#endif
    else if ( x != 2 )
      setPixelRGB(start, CRGBBlack, true, 0);
  }

  //setPixelHSV(random16_min_max(start, start + count), (cHSV){cvars->cycle_hue, 1, 255}, true, cvars->dim);
}

typedef struct
{
  twinkleState state;
  uint8_t brightness;
} oot_t;
uint8_t ootOffset[] = {1,2,3,4,5,4,3,2};
#define OOTOFFSET_LEN (sizeof(ootOffset) / sizeof(uint8_t))
void out_of_time(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static oot_t *ootPixel = NULL;
  static uint16_t tosw = 1;
  static int8_t speed = 1;

  // Initialisation after patten change
  if ( cvars->cur_pattern != cvars->pre_pattern )
  {
    // Set our pattern delay
    cvars->delay_ticks = DELAY_OUTOFTIME;
  }

  // First run initialisition
  if ( ootPixel == NULL )
  {
    F_LOGI(true, true, LC_GREY, "Initialising data...");
    ootPixel = (oot_t *)pvPortMalloc(sizeof(oot_t) * count);
    memset(ootPixel, 0, sizeof(oot_t) * count);
  }

  if ( --tosw <= 1 )
  {
    tosw  = prng() % 500 + 10;
    speed = (prng() % 2) + 1;
  }

  uint16_t curPos = start;
  for ( uint16_t i = 0; i < count; i++ )
  {
    // If blend
    //cHSV pixel = getPixelHSV(i);

    // If switch
    cHSV pixel = hsvFromPalette(curPalette[cvars->thisPalette], curPos, INI_BRIGHTNESS);

    // Temporary current speed
    uint8_t cspeed = speed + ootOffset[i % OOTOFFSET_LEN];

    if ( ootPixel[curPos].state == Init )
    {
      ootPixel[curPos].state      = GettingBrighter;
      ootPixel[curPos].brightness = MIN_BRIGHTNESS;
    }
    else if ( ootPixel[curPos].state == GettingBrighter )
    {
      if ( int(ootPixel[curPos].brightness + cspeed) >= MAX_BRIGHTNESS )
      {
        ootPixel[curPos].brightness = MAX_BRIGHTNESS;
        ootPixel[curPos].state      = GettingDimmer;
      }
      else
      {
        ootPixel[curPos].brightness += cspeed;
      }
    }
    else if ( ootPixel[curPos].state == GettingDimmer )
    {
      if ( int(ootPixel[curPos].brightness - cspeed) <= MIN_BRIGHTNESS )
      {
        ootPixel[curPos].brightness = MIN_BRIGHTNESS;
        ootPixel[curPos].state      = GettingBrighter;
      }
      else
      {
        ootPixel[curPos].brightness -= cspeed;
      }
    }

    pixel.v = ease8InOutQuad(ootPixel[curPos].brightness);
    //pixel.v = cubicwave8(ootPixel[curPos].brightness);
    setPixelHSV(curPos, pixel, true, cvars->dim);

    curPos = inc_curpos(curPos, start, count);
  }
}

void pride(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // Set our pattern delay
  cvars->delay_ticks = 15;

  uint8_t sat8 = beatsin88(87, 220, 250, 5, 0);
  uint8_t brightdepth = beatsin88(341, 96, 224, 5, 0);
  uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256), 5, 0);
  uint8_t msmultiplier = beatsin88(147, 23, 60, 5, 0);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000, 5, 0);

  uint16_t ms = esp_timer_get_time();
  uint16_t deltams = ms - sLastMillis;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88(400, 5, 9, 5, 0);
  uint16_t brightnesstheta16 = sPseudotime;

  uint16_t curPos = start;
  for ( uint16_t i = 0; i < count; i++ )
  {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16(brightnesstheta16) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    setPixelHSV(curPos, {hue8, sat8, bri8}, true, cvars->dim);

    curPos = inc_curpos(curPos, start, count);
  }
}

//*****************************************************************
//***********************  Test Pattern  **************************
//*****************************************************************
//#define PRINT_RES   true
void test_pattern(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  // Runtime stats
  static uint32_t loops      = 0;            // Iterations
  static uint32_t code_tot   = 0;            // Cumaltive time running code
  static uint16_t y = 20;

  cvars->delay_ticks = 200;

  uint64_t loop_start = esp_timer_get_time();
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Insert code to be timed.
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  cRGB a, b;

  a = Cyan;
  b = Yellow;
  setPixelRGB(1, nblend(a, b, 127), true, cvars->dim);
  a = Yellow;
  b = Cyan;
  setPixelRGB(2, nblend(a, b, 127), true, cvars->dim);

  a = Magenta;
  b = Yellow;
  setPixelRGB(3, nblend(a, b, 220), true, cvars->dim);
  a = Magenta;
  b = Yellow;
  setPixelRGB(4, nblend(a, b, 220), true, cvars->dim);

  a = Cyan;
  b = Magenta;
  setPixelRGB(5, nblend(a, b,220), true, cvars->dim);
  a = Magenta;
  b = Cyan;
  setPixelRGB(6, nblend(a, b, 220), true, cvars->dim);

  a = Black;
  b = Blue;
  setPixelRGB(8, nblend(a, b,220), true, cvars->dim);
  a = Blue;
  b = Black;
  setPixelRGB(9, nblend(a, b, 220), true, cvars->dim);

  for ( int i = 0, c = 1; i < 500; i++ )
  {
    //int x = sub8(5, 12);          // old, 60237us (an average of 60us per iteration)
    //int x = sub8(5, 12);          // new, 57277us (an average of 57us per iteration)

    //uint16_t x = avg7(127,127);
    //uint8_t x = scale8(156,156);


#if defined(PRINT_RES)
    if ( y == 0 )
    {
      if ( c <= 10 )
      {
        if ( (i % 255) == 0 )
        {
          printf(" 0x%04X (%d)", x, x);
          //printf(" %2d: (%2d, %.4f)", i, x, f);
          c++;
        }
      }
    }
#endif
  }
#if defined(PRINT_RES)
  if ( y == 0 )
  {
    printf("\n");
    y = 20;
  }
  else
  {
    y--;
  }
#endif
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // End of timed code.
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  code_tot += (esp_timer_get_time() - loop_start);
  if ( ++loops == 1000 )
  {
    F_LOGI(true, true, LC_MAGENTA, "After %d iterations, test code took %dus (an average of %dus per iteration)", loops, code_tot, (int)(code_tot / loops));
    loops      = 0;
    code_tot   = 0;
  }
}

// *****************************************************************************************
// * Template for creating new patterns                                                    *
// *****************************************************************************************
/*
void pattern_template(uint16_t start, uint16_t count, uint8_t step, controlvars_t *cvars)
{
  static uint8_t *avar = NULL;

  // Initialisation after patten change
  if ( cvars->cur_pattern != cvars->pre_pattern )
  {
    // Set our pattern delay
    cvars->delay_ticks = 0;
  }

  // First run initialisition
  // First time through we only initialise, but after that, we save on comparisons
  if ( avar == NULL )
  {
    F_LOGI(true, true, LC_GREY, "Initialising data...");
    avar = (uint8_t *) pvPortMalloc (sizeof(uint8_t) * count);
    memset (avar, 0, sizeof(uint8_t));
  }
  else
  {
    uint16_t curPos = start;
    for ( uint16_t i = 0; i < count; i++ )
    {
      // code
      curPos = inc_curpos(curPos, start, count);
    }
  }
}
*/
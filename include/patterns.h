#ifndef   __PATTERNS_H__
#define   __PATTERNS_H__


#define DELAY_POLICE_LIGHTS       1
#define DELAY_CIRCULAR            1
#define DELAY_RACHELS             0
#define DELAY_NHSBLUE             1
#define DELAY_WELSHPRIDE          1
#define DELAY_TWINKLEMAP          2
#define DELAY_COLORWAVES          0
#define DELAY_FADEINANDOUT        2
#define DELAY_BOUNCINGBALLS       1
#define DELAY_BLOCKY              20
#define DELAY_NULLPATTERN         50
#define DELAY_DAILY_COLOUR        50
#define DELAY_OLDSCHOOL           1
#define DELAY_OLDSCHOOL_ROTATE    75
#define DELAY_TRADITIONAL         3
#define DELAY_RAINBOW             2
#define DELAY_COLORWIPE_ONE       1
#define DELAY_COLORWIPE_TWO       1
#define DELAY_UNDULATE            25
#define DELAY_CANDYCORN           5
#define DELAY_PALETTESCROLL       25
#define DELAY_METEOR              0
#define DELAY_MATRIX              1
#define DELAY_ONESIN              0
#define DELAY_TWOSIN              0
#define DELAY_JUGGLE              18
#define DELAY_DISCOSTROBE         2
#define DELAY_LARSENSCANNER       2
#define DELAY_LONEWOLF            2
#define DELAY_DOUBLECHASER        2
#define DELAY_RING                1
#define DELAY_PLASMA              3
#define DELAY_SIDERAIN            3
#define DELAY_OUTOFTIME           20

typedef struct
{
  uint16_t  hue_min;
  uint16_t  hue_max;
} _hue_t;
typedef enum hue_color
{
  HUE_RED,
  HUE_YELLOW,
  HUE_GREEN,
  HUE_CYAN,
  HUE_BLUE,
  HUE_MAGENTA,
  HUE_ALL,
  HUE_MAX
} _hue_color_t;

typedef struct
{
  uint16_t    pixel;
  cHSV        colour;
  uint8_t     counter;
  uint8_t     speed;
  uint8_t     on;
} starStruc;

// Main pattern functions
void        blocky(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        plasma(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        bouncingballs(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        candycorn(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        circular(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        colorwipe_1(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        colorwipe_2(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        daily_colour(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        discostrobe(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        doubleChaser(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        fadeinandout(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        juggle(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        loneWolf(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        larsenScanner(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        matrix(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        null_pattern(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        oldschool(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        oldschoolRotate(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        one_sin_init(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        palettescroll(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        police_lights(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        rachels(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        rainbow_one(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        rainbow_two(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        traditional(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        test_pattern(uint16_t, uint16_t, uint8_t, controlvars_t * );
void        twinklemappixels(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        two_col_init(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        two_sin_init(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        undulate(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        color_waves(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        side_rain(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        meteorRain(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        out_of_time(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        pride(uint16_t, uint16_t, uint8_t, controlvars_t *);
void        solid_colour (uint16_t start, uint16_t count, uint8_t step, cRGB colour, controlvars_t *);


// Pattern sub functions
void        one_sin(uint16_t, uint16_t, uint8_t, controlvars_t *, bool oneColour);
void        two_sin(uint16_t, uint16_t, uint8_t, controlvars_t *, cHSV cola, cHSV colb);

// Sparkles, dazzles and other fancy adornments.
bool        dazzle(controlvars_t *, uint8_t, dazzle_mode mode);

// Private - Move to 'c' file
void        discoworker(controlvars_t *, uint8_t, uint8_t, int8_t, uint8_t, uint8_t);
void        drawRainbowDashes(controlvars_t *, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void        drawFractionalBar( int pos16, int width, uint8_t hue, bool wrap, uint16_t start, uint16_t count, uint8_t dim );
void        Ring( uint16_t start, uint16_t count, uint16_t animationFrame, uint8_t hue, uint8_t dim );

#endif

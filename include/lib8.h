#ifndef   __LIB8_H__
#define   __LIB8_H__

#define qsubd(x, b)  ((x>b)?b:0)                                                                   // A digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.
#define qsuba(x, b)  ((x>b)?x-b:0)                                                                 // Unsigned subtraction macro. if result <0, then => 0
#define arr_len(x) (sizeof(x) / sizeof(x[0]))

#define scale8_LEAVING_R1_DIRTY(x,y) ((int) x * (int)(y) ) >> 8
#define nscale8_LEAVING_R1_DIRTY(x,y) ((int)x * (int)(y) ) >> 8;


/// ANSI unsigned short _Fract.  range is 0 to 0.99609375
///                 in steps of 0.00390625
typedef uint8_t fract8;         ///< ANSI: unsigned short _Fract

///  ANSI: signed short _Fract.  range is -0.9921875 to 0.9921875
///                 in steps of 0.0078125
typedef int8_t sfract7;         ///< ANSI: signed   short _Fract

///  ANSI: unsigned _Fract.  range is 0 to 0.99998474121
///                 in steps of 0.00001525878
typedef uint16_t fract16;       ///< ANSI: unsigned       _Fract

///  ANSI: signed _Fract.  range is -0.99996948242 to 0.99996948242
///                 in steps of 0.00003051757
typedef int16_t sfract15;       ///< ANSI: signed         _Fract

typedef uint16_t accum88;       ///< ANSI: unsigned short _Accum.  8 bits int, 8 bits fraction
typedef int16_t saccum78;       ///< ANSI: signed   short _Accum.  7 bits int, 8 bits fraction
typedef uint32_t accum1616;     ///< ANSI: signed         _Accum. 16 bits int, 16 bits fraction
typedef int32_t saccum1516;     ///< ANSI: signed         _Accum. 15 bits int, 16 bits fraction
typedef uint16_t accum124;      ///< no direct ANSI counterpart. 12 bits int, 4 bits fraction
typedef int32_t saccum114;      ///< no direct ANSI counterpart. 1 bit int, 14 bits fraction





uint8_t     beat8( accum88 beats_per_minute, uint32_t timebase );
uint16_t    beat88( accum88 beats_per_minute_88, uint32_t timebase );
uint16_t    beat16( accum88 beats_per_minute, uint32_t timebase );
uint8_t     beatsin8( accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase, uint8_t phase_offset );
uint16_t    beatsin88( accum88 beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset );
uint16_t    beatsin16( accum88 beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset );

uint8_t     triwave8( uint8_t in );
uint8_t     quadwave8( uint8_t in );
uint8_t     cubicwave8( uint8_t in );
uint8_t     squarewave8( uint8_t in, uint8_t pulsewidth );

uint8_t     ease8InOutQuad( uint8_t i );
fract8      ease8InOutCubic( fract8 i );
fract8      ease8InOutApprox( fract8 i );
uint8_t     ease8InOutApprox( fract8 i );

#endif

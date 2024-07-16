#ifndef __MOONPHASE_H__
#define __MOONPHASE_H__

typedef enum
{
  PHASE_NEW = 0,
  PHASE_WAXING_CRESCENT,
  PHASE_FIRST_QUARTER,
  PHASE_WAXING_GIBBOUS,
  PHASE_FULL,
  PHASE_WANING_GIBBOUS,
  PHASE_THIRD_QUARTER,
  PHASE_WANING_CRESCENT
} moonphase_t;

typedef struct
{
  double         age;
  moonphase_t   phase;
  const char    *nameStr;
} phaseinfo_t;

int get_moonphase(phaseinfo_t *phaseData);

#endif // __MOONPHASE_H__
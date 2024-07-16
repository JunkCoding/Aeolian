#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "moonphase.h"

// 29.53, 29.5306, 29.530589, 29.530588853
#define SYNODIC_MONTH	  29.530588853
// Full moon Thursday, January 6th, 18:14, 2000 (Unix TS: 947182440)
#define FULL_MOON_2000  947182440

// An array of moon phases
const char *phaseStr[] =
{
  "New",
  "Waxing Crescent",
  "First Quarter",
  "Waxing Gibbous",
  "Full",
  "Waning Gibbous",
  "Third Quarter",
  "Waning Crescent"
};

// Compile with 'gcc get-moon-phase.c -lm -o get-moon-phase.o'
int get_moonphase(phaseinfo_t *phaseData)
{
  double currentSec = fmod(((unsigned long int) time(NULL) - FULL_MOON_2000), (SYNODIC_MONTH * 86400));
  double currentFra = currentSec / (SYNODIC_MONTH * 86400);

  // Current day of the lunar month (moon age)
  phaseData->age = currentFra * SYNODIC_MONTH;

  // Current Phase
  if ( phaseData->age < 1.845662 )
  {
    phaseData->phase = PHASE_NEW;
  }
  else if ( phaseData->age < 5.536986 )
  {
    phaseData->phase = PHASE_WAXING_CRESCENT;
  }
  else if ( phaseData->age < 9.228310 )
  {
    phaseData->phase = PHASE_FIRST_QUARTER;
  }
  else if ( phaseData->age < 12.919634 )
  {
    phaseData->phase = PHASE_WAXING_GIBBOUS;
  }
  else if ( phaseData->age < 16.610958 )
  {
    phaseData->phase = PHASE_FULL;
  }
  else if ( phaseData->age < 20.302282 )
  {
    phaseData->phase = PHASE_WANING_GIBBOUS;
  }
  else if ( phaseData->age < 23.993606 )
  {
    phaseData->phase = PHASE_THIRD_QUARTER;
  }
  else if ( phaseData->age < 27.684930 )
  {
    phaseData->phase = PHASE_WANING_CRESCENT;
  }
  else
  {
    phaseData->phase = PHASE_NEW;
  }

  // Phase name string
  phaseData->nameStr = phaseStr[phaseData->phase];

  return 0;
}


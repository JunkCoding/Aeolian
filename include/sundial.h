#ifndef __SUNDIAL_H__
#define __SUNDIAL_H__

double sunrise(double latitude,double longitude, time_t relativeTo);
double sunset(double latitude,double longitude, time_t relativeTo);
// double getTime( time_t relative);
#define getTime(x) ((double)(x))

#endif // SUNDIAL_H

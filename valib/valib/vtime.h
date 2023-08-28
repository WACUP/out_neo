#ifndef VALIB_VTIME_H
#define VALIB_VTIME_H

#include "defs.h"

// inline vtime_t cpu_time();          // total CPU time used by this thread

///////////////////////////////////////////////////////////////////////////////
// This functions return the number of seconds elapsed since midnight 
// (00:00:00), January 1, 1970, according to the system clock. So this value 
// can be easily converted to most common integer time format just by 
// type-casting.

// This functions may return time in coarse units (up to 1sec), so it is not
// recommended to use it as precise values.

vtime_t utc_time();          // current system time, UTC
vtime_t local_time();        // current system time, local time

// This function may do conversion with coarse units so it is not recommended
// to make conversions of precise values.

vtime_t to_local(vtime_t); // convert UTC time to local time
vtime_t to_utc(vtime_t);   // convert local time to UTC time

#endif

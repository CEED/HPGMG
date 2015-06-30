//------------------------------------------------------------------------------------------------------------------------------
// Samuel Williams
// SWWilliams@lbl.gov
// Lawrence Berkeley National Lab
//------------------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#define CALIBRATE_TIMER // mg.c will calibrate the timer to determine seconds per cycle
uint64_t CycleTime(){
  uint64_t lo, hi;
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return( (((uint64_t)hi) << 32) | ((uint64_t)lo) );
}

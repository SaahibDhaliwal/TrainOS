#ifndef __TIMER__
#define __TIMER__
#include <stdint.h>

uint32_t timerInit();  // returns the time when the tick is initialized
unsigned int timerGet();
unsigned int timerGetTick();
void timerSetNextTick();
unsigned int timerGetRelativeTime();

#endif
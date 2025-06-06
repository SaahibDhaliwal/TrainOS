#ifndef __TIMER__
#define __TIMER__
#include <stdint.h>

void timerInit();
unsigned int timerGet();
void timerSetNextTick();

#endif
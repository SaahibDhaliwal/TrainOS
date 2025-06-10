#ifndef __TIMER__
#define __TIMER__
#include <stdint.h>

void timerInit();
uint32_t timerGet();
void timerSetNextTick();

#endif
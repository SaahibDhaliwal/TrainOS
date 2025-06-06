#ifndef __CLOCK_SERVER__
#define __CLOCK_SERVER__

void ClockFirstUserTask();

void ClockServer();

int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);

#endif
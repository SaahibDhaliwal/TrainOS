#ifndef __CS_PROTOCOL__
#define __CS_PROTOCOL__

namespace clock_server {
int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);
}  // namespace clock_server

#endif
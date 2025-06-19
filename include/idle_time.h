#ifndef __IDLE_TIME__
#define __IDLE_TIME__

#include <stddef.h>
#include <stdint.h>

void print_idle_percentage();
// void update_idle_percentage(int idlePercent);
void update_idle_percentage(int percentage, int printTID);

#endif  // idle_time.h

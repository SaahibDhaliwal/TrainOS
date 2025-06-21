#ifndef __UPTIME__
#define __UPTIME__

#include <stddef.h>
#include <stdint.h>

void print_uptime(int consoleTid);
void print_uptime();

void update_uptime(int consoleTid, uint64_t micros);

#endif  // uptime.h

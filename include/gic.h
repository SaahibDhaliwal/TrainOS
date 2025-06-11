#ifndef __GIC__
#define __GIC__
#include <stdint.h>

void gicInit();

void gicEndInterrupt(int interrupt_id);

uint32_t gicGetInterrupt(void);

#endif
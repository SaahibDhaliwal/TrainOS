#include <gic.h>
#include <stdarg.h>
#include <stdint.h>

#include "config.h"

/*********** TIMER CONTROL ************************ ************/
static char* const TIMER_BASE = (char*)(0xFE000000 + 0x3000);  // would need to change if MMIO_BASE changes
static const uint32_t TIMER_CONTROL = 0x00;
static const uint32_t TIMER_LO = 0x04;
static const uint32_t TIMER_HI = 0x08;
static const uint32_t TIMER_COMPARE_1 = 0x10;

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))
unsigned int first_tick_time = 0;

unsigned int timerGet() {
    return TIMER_REG(TIMER_LO);
}

unsigned int timerGetTick() {
    return (TIMER_REG(TIMER_LO) - (first_tick_time - Config::TICK_SIZE)) / Config::TICK_SIZE;
}

unsigned int timerInit() {
    first_tick_time = TIMER_REG(TIMER_LO) + Config::TICK_SIZE;
    TIMER_REG(TIMER_COMPARE_1) = first_tick_time;
    return first_tick_time;
}

void timerSetNextTick() {
    // Take elapsed time
    // divide by tick interval (to get how many total ticks have passed)
    // add one more tick
    // multiply by tick interval (to convert back into time)
    // Ensures alarm is synchronized on our tick size.
    // Note: does not handle timer overflow of TIMER_LO (I think)
    TIMER_REG(TIMER_COMPARE_1) +=
        ((TIMER_REG(TIMER_LO) - TIMER_REG(TIMER_COMPARE_1)) / Config::TICK_SIZE + 1) * Config::TICK_SIZE;
    TIMER_REG(TIMER_CONTROL) |= (0x1 << 1);  // writes a 1 to M1 to clear the match detect status bit
}

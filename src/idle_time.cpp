#include "idle_time.h"

#include <stdint.h>
#include <string.h>

#include "console_server_protocol.h"
#include "cursor.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "rpi.h"

#define IDLE_START_ROW 13
#define IDLE_START_COL 0
#define IDLE_LABEL "Idle Time: "

static char* const TIMER_BASE = (char*)(0xfe003000);

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))

void print_idle_percentage(int printTid) {
    console_server::Printf(printTid, "\033[%d;%dH%s", IDLE_START_ROW, IDLE_START_COL, IDLE_LABEL);
}

void update_idle_percentage(int percentage, int printTid) {
    console_server::Printf(printTid, "\033[%d;%dH%d.%d%%  ", IDLE_START_ROW, IDLE_START_COL + strlen(IDLE_LABEL),
                           percentage / 100,
                           percentage % 100);  // padded to get rid of excess
}

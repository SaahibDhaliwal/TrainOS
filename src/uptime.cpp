#include "uptime.h"

#include <stdint.h>
#include <string.h>

#include "console_server_protocol.h"
#include "cursor.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"

#define UPTIME_START_ROW 13
#define UPTIME_START_COL 45
#define UPTIME_LABEL "Uptime: "

static char* const TIMER_BASE = (char*)(0xfe003000);

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))

void print_uptime(int consoleTid) {
    printer_proprietor::printF(consoleTid, "\033[s\033[%d;%dH%s\033[u", UPTIME_START_ROW, UPTIME_START_COL,
                               UPTIME_LABEL);
}

void update_uptime(int printTid, uint64_t micros) {
    uint64_t millis = micros / 1000;
    uint32_t tenths = (millis % 1000) / 100;
    uint32_t seconds = (millis / 1000) % 60;
    uint32_t minutes = (millis / (1000 * 60)) % 60;
    uint32_t hours = millis / (1000 * 60 * 60);

    printer_proprietor::printF(printTid, "\033[s\033[%d;%dH%dh %dm %d.%ds   \033[u", UPTIME_START_ROW,
                               UPTIME_START_COL + strlen(UPTIME_LABEL), hours, minutes, seconds,
                               tenths);  // padded to get rid of excess
}

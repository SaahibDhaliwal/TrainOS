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
#define UPTIME_LABEL_LENGTH 53

static char* const TIMER_BASE = (char*)(0xfe003000);

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))

void print_uptime(int consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH%s", UPTIME_START_ROW, UPTIME_START_COL, UPTIME_LABEL);
}

void update_uptime(int printTid, uint64_t micros) {
    uint64_t millis = micros / 1000;
    uint32_t tenths = (millis % 1000) / 100;
    uint32_t seconds = (millis / 1000) % 60;
    uint32_t minutes = (millis / (1000 * 60)) % 60;
    uint32_t hours = millis / (1000 * 60 * 60);

    console_server::Printf(printTid, "\033[%d;%dH%dh %dm %d.%ds   ", UPTIME_START_ROW, UPTIME_LABEL_LENGTH, hours,
                           minutes, seconds,
                           tenths);  // padded to get rid of excess
}

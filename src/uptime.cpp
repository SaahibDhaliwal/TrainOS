#include "uptime.h"

#include <stdint.h>
#include <string.h>

#include "cursor.h"
#include "protocols/ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "servers/console_server.h"

#define UPTIME_START_ROW 2
#define UPTIME_START_COL 50
#define UPTIME_LABEL "Uptime: "

static char* const TIMER_BASE = (char*)(0xfe003000);

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))

void print_uptime(int consoleTid) {
    uartPrintf(consoleTid, "\033[%d;%dH", UPTIME_START_ROW, UPTIME_START_COL);
    uartPrintf(consoleTid, UPTIME_LABEL);
}

void print_uptime() {
    uart_printf(CONSOLE, "\033[%d;%dH", UPTIME_START_ROW, UPTIME_START_COL);
    uart_printf(CONSOLE, UPTIME_LABEL);
}

void broken_print(int consoleTid, uint64_t micros) {
    unsigned int t = micros / 1000;
    int allseconds = t / 1000;
    int minutes = allseconds / 60;
    int seconds = allseconds % 60;
    int tenth = (t / 100) % 10;

    const int baseRow = UPTIME_START_ROW;
    const int baseCol = UPTIME_START_COL + strlen(UPTIME_LABEL);

    // Print minutes
    uartPrintf(consoleTid, "\033[%d;%dH", baseRow, baseCol);
    if (minutes < 10) uartPutConsoleS(consoleTid, "0");
    uartPrintf(consoleTid, "%d", minutes);

    // Print ':'
    uartPrintf(consoleTid, "\033[%d;%dH:", baseRow, baseCol + 2);

    // Print seconds
    uartPrintf(consoleTid, "\033[%d;%dH", baseRow, baseCol + 3);
    if (seconds < 10) uartPutConsoleS(consoleTid, "0");
    uartPrintf(consoleTid, "%d", seconds);

    // Print '.'
    uartPrintf(consoleTid, "\033[%d;%dH.", baseRow, baseCol + 5);

    // Print tenths
    uartPrintf(consoleTid, "\033[%d;%dH%d", baseRow, baseCol + 6, tenth);
}

void update_uptime(int printTid, uint64_t micros) {
    //     uint64_t millis = micros / 1000;
    //     uint32_t tenths = (millis % 1000) / 100;
    //     uint32_t seconds = (millis / 1000) % 60;
    //     uint32_t minutes = (millis / (1000 * 60)) % 60;
    //     uint32_t hours = millis / (1000 * 60 * 60);

    //     uartPrintf(printTid, "\033[%d;%dH", UPTIME_START_ROW, UPTIME_START_COL + strlen(UPTIME_LABEL));
    //     uartPrintf(printTid, "%dh %dm %d.%ds   ", hours, minutes, seconds,
    //                tenths);  // padded to get rid of excess
    broken_print(printTid, micros);
}

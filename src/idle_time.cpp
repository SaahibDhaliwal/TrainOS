#include "idle_time.h"

#include <stdint.h>
#include <string.h>

#include "cursor.h"
#include "protocols/ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "servers/console_server.h"

#define IDLE_START_ROW 0
#define IDLE_START_COL 0
#define IDLE_LABEL "Idle Time: "

static char* const TIMER_BASE = (char*)(0xfe003000);

#define TIMER_REG(offset) (*(volatile uint32_t*)(TIMER_BASE + offset))

void print_idle_percentage() {
    uart_printf(CONSOLE, "\033[%d;%dH", IDLE_START_ROW, IDLE_START_COL);
    uart_printf(CONSOLE, IDLE_LABEL);
}

void update_idle_percentage(int percentage) {
    int ctid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    // uartPrintf(ctid, "\033[%d;%dH", IDLE_START_ROW, IDLE_START_COL + strlen(IDLE_LABEL));
    uart_printf(CONSOLE, "\033[%d;%dH", IDLE_START_ROW, IDLE_START_COL + strlen(IDLE_LABEL));
    // clear
    // uartPrintf(ctid, "\033[K");  // clear from cursor to end of line
    uart_printf(CONSOLE, "\033[K");  // clear from cursor to end of line

    // uartPrintf(ctid, " %d.%d", percentage / 100, percentage % 100);
    uart_printf(CONSOLE, " %d.%d", percentage / 100, percentage % 100);

    uart_putc(CONSOLE, '%');
}

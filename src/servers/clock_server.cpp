#include "servers/clock_server.h"

#include "rpi.h"
#include "servers/name_server.h"
#include "sys_call_stubs.h"
#include "timer.h"

// bit vs byte offset

void ClockFirstUserTask() {
    // uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(9, &NameServer));
    // uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\n\r", Create(8, &ClockServer));
    uart_printf(CONSOLE, "Started first clock task\n\r");
    timerInit();
    sys::Exit();
}

void ClockNotifier() {
    // waits on events from a timer, and sends notifications that the timer has ticked
}

void ClockClient() {
    // Send request for parameters

    // Discover TID of clock server

    // Delay N times for each interval t.
}

void ClockServer() {
}

int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);

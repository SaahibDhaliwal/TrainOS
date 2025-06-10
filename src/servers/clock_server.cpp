#include "servers/clock_server.h"

#include "interrupt.h"
#include "protocols/generic_protocol.h"
#include "protocols/ns_protocol.h"
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
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    for (;;) {
        sys::AwaitEvent(static_cast<int>(INTERRUPT_NUM::CLOCK));
        emptySend(clockServerTid);
    }
}

void ClockClient() {
    // Send request for parameters

    // Discover TID of clock server

    // Delay N times for each interval t.
}

void ClockServer() {
    // create clock notifier, so i know the tid

    // if a receive a message and tid is clock notifier i know what to do
}

int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);

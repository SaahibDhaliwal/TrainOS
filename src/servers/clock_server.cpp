#include "servers/clock_server.h"

#include "rpi.h"
#include "servers/name_server.h"
#include "sys_call_stubs.h"
#include "timer.h"

// bit vs byte offset

void ClockFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", sys::Create(9, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\n\r", sys::Create(8, &ClockServer));

    sys::Create(20, &ClockClient);
    sys::Create(19, &ClockClient);
    sys::Create(18, &ClockClient);
    sys::Create(17, &ClockClient);

    // uart_printf(CONSOLE, "Started first clock task\n\r");
    // timerInit();
    sys::Exit();
}

void ClockNotifier() {
    // waits on events from a timer, and sends notifications that the timer has ticked
}

void ClockClient() {
    // Send request for parameters
    char* param_msg = "Give me params";
    char reply[32] = {0};
    int reply_length = sys::Send(sys::MyParentTid(), param_msg, strlen(param_msg), reply, strlen(reply));

    // Discover TID of clock server

    // Delay N times for each interval t.
}

void ClockServer() {
    // initialize the timer, get the value of the first tick

    // for(::){

    //}
}

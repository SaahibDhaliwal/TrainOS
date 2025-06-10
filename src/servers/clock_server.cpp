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
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    for (;;) {
        sys::AwaitEvent(static_cast<int>(INTERRUPT_NUM::CLOCK));
        emptySend(clockServerTid);
    }
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
    int registerReturn = name_server::RegisterAs(CLOCK_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER \n\r");
        sys::Exit();
    }

    uint64_t ticks = 0;
    int32_t clockNotifierTid = sys::Create(4, &ClockNotifier);

    for (;;) {
        uint32_t senderTid;
        // change this to something reasonable, not sure what the protocols look like yet
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];

        int msgLen = sys::Receive(&senderTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';

        if (senderTid == clockNotifierTid) {
            ticks += 1;
            uart_printf(CONSOLE, "Current ticks: %u, Current time: %u\n\r", ticks, timerGet());
            charReply(clockNotifierTid, '0');
        }
        }
    // create clock notifier, so i know the tid

    // if a receive a message and tid is clock notifier i know what to do
}

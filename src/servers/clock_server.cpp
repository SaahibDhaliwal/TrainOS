#include "clock_server.h"

#include "config.h"
#include "cs_protocol.h"
#include "generic_protocol.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "name_server.h"
#include "ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
// bit vs byte offset

using namespace clock_server;

namespace {
class DelayedClockClient : public IntrusiveNode<DelayedClockClient> {
   public:
    uint32_t tid;
    uint64_t releaseTime;
    bool active = false;

    void initialize(uint32_t newTid, uint64_t newReleaseTime) {
        tid = newTid;
        releaseTime = newReleaseTime;
        active = true;
    }
};
}  // namespace

void ClockFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", sys::Create(49, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\n\r", sys::Create(50, &ClockServer));

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);

    uint32_t client_1 = sys::Create(20, &ClockClient);
    uint32_t client_2 = sys::Create(19, &ClockClient);
    uint32_t client_3 = sys::Create(18, &ClockClient);
    uint32_t client_4 = sys::Create(17, &ClockClient);

    uint32_t client_tid = 0;
    char parentMsg[4] = {0};
    sys::Receive(&client_tid, parentMsg, 3);
    ASSERT(client_tid == client_1);
    sys::Reply(client_tid, "1020", 4);

    sys::Receive(&client_tid, parentMsg, 3);
    ASSERT(client_tid == client_2);
    sys::Reply(client_tid, "2309", 4);

    sys::Receive(&client_tid, parentMsg, 3);
    ASSERT(client_tid == client_3);
    sys::Reply(client_tid, "3306", 4);

    sys::Receive(&client_tid, parentMsg, 3);
    ASSERT(client_tid == client_4);
    sys::Reply(client_tid, "7103", 4);

    uart_printf(CONSOLE, "First User Task: Done \n\r");

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
    // char* param_msg = "Give me params";

    char reply[4] = {0};
    int reply_length = sys::Send(sys::MyParentTid(), "", 0, reply, 4);
    ASSERT(reply_length >= 4);

    int delay_interval = a2d(reply[0]) * 10 + a2d(reply[1]);
    int delay_count = a2d(reply[2]) * 10 + a2d(reply[3]);

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);

    int tid = sys::MyTid();

    for (int i = 0; i < delay_count; i++) {
        clock_server::Delay(clockServerTid, delay_interval);
        uart_printf(CONSOLE, "[Clock Client %u]: Delay Interval: %u Completed Delays: %u \n\r", tid, delay_interval,
                    i + 1);
    }

    sys::Exit();
}

void ClockServer() {
    timerInit();
    int registerReturn = name_server::RegisterAs(CLOCK_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER \n\r");
        sys::Exit();
    }

    DelayedClockClient clockClientSlabs[Config::MAX_TASKS];
    Stack<DelayedClockClient> freelist;

    // initialize our structs
    for (int i = 0; i < Config::MAX_TASKS; i += 1) {
        freelist.push(&clockClientSlabs[i]);
    }

    uint64_t ticks = 0;
    int32_t clockNotifierTid = sys::Create(40, &ClockNotifier);

    for (;;) {
        uint32_t clientTid;
        char msg[Config::MAX_MESSAGE_LENGTH];
        int msgLen = sys::Receive(&clientTid, msg, 2);
        msg[msgLen] = '\0';

        if (clientTid == clockNotifierTid) {
            ticks += 1;
            charReply(clockNotifierTid, '0');

            for (int i = 0; i < Config::MAX_TASKS; i += 1) {
                if (clockClientSlabs[i].active && ticks >= clockClientSlabs[i].releaseTime) {
                    clockClientSlabs[i].active = false;
                    freelist.push(&clockClientSlabs[i]);
                    uIntReply(clockClientSlabs[i].tid, ticks);
                }
            }
        } else {
            Command command = commandFromByte(msg[0]);

            switch (command) {
                case Command::TIME: {
                    uIntReply(clientTid, ticks);

                    break;
                }
                case Command::DELAY: {
                    char tickString[21];
                    strcpy(tickString, &msg[1]);

                    unsigned int delayTicks;
                    a2ui(tickString, 10, &delayTicks);

                    DelayedClockClient* delayedClient = freelist.pop();
                    delayedClient->initialize(clientTid, ticks + delayTicks);

                    break;
                }
                case Command::DELAY_UNTIL: {
                    char tickString[21];
                    strcpy(tickString, &msg[1]);

                    unsigned int endTicks;
                    a2ui(tickString, 10, &endTicks);

                    DelayedClockClient* delayedClient = freelist.pop();
                    delayedClient->initialize(clientTid, endTicks);

                    break;
                }
                default: {
                    uart_printf(CONSOLE, "[Clock Server]: Unknown Command!\n\r");

                    break;
                }
            }
        }
    }
    // create clock notifier, so i know the tid

    // if a receive a message and tid is clock notifier i know what to do
}

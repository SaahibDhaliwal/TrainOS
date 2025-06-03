#include "user_tasks.h"

#include <string.h>

#include <cstdint>
#include <map>

#include "fixed_map.h"
#include "name_server.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "util.h"

void FirstUserTask() {
    //  Assumes FirstUserTask is at Priority 2
    const uint64_t lowerPrio = 2 - 1;
    const uint64_t higherPrio = 2 + 1;

    // create two tasks at a lower priority
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));

    // // create two tasks at a higher priority
    uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, &TestTask));

    uart_printf(CONSOLE, "FirstUserTask: exiting\n\r");
    Exit();
}

void TestTask() {
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
    Yield();
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
    Exit();
}

void IdleTask() {
    while (true) {
        asm volatile("wfe");
    }
}

void SenderTask() {
    char* msg =
        "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum "
        "sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies "
        "nec, pellentesque eu, pretium quis,.";

    int start = 0;
    int end = 0;

    char reply[Config::MAX_MESSAGE_LENGTH];
    start = get_timer();
    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        int replylen = Send(3, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    }
    end = get_timer();
    uart_printf(CONSOLE, "clock time diff %u \n\r", (end - start) / Config::EXPERIMENT_COUNT);

    Exit();
}

void ReceiverTask() {
    // RegisterAs("receiver");

    // //this won't work as intended
    // for(;;){
    //     int receiverTID = WhoIs("sender");
    //     if(receiverTID != -1) break;
    //     Yield();
    // }
    // int receiverTID = WhoIs("receiver");

    char reply[Config::MAX_MESSAGE_LENGTH];

    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        uint32_t sender;
        char buffer[Config::MAX_MESSAGE_LENGTH];

        int msgLen = Receive(&sender, buffer, Config::MAX_MESSAGE_LENGTH);
        // uart_printf(CONSOLE, "Received Message! \n\r", buffer);
        Reply(sender, "Got Message", Config::MAX_MESSAGE_LENGTH);
        // uart_printf(CONSOLE, "Sent Reply! \n\r");
    }
    Exit();
}

void PerformanceMeasurement() {
    // uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(10, &NameServer));

    uart_printf(CONSOLE, "[First Task]: Created Sender: %u\n\r", Create(9, &SenderTask));
    uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\n\r", Create(10, &ReceiverTask));
    Exit();
}

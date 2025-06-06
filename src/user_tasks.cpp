#include "user_tasks.h"

#include <string.h>

#include <cstdint>
#include <map>

#include "clients/rps_client.h"
#include "fixed_map.h"
#include "queue.h"
#include "rpi.h"
#include "servers/name_server.h"
#include "servers/rps_server.h"
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
    char reply[Config::MAX_MESSAGE_LENGTH];

    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        uint32_t sender;
        char buffer[Config::MAX_MESSAGE_LENGTH];
        int msgLen = Receive(&sender, buffer, Config::MAX_MESSAGE_LENGTH);
        Reply(sender, "Got Message", Config::MAX_MESSAGE_LENGTH);
    }
    Exit();
}

void PerformanceMeasurement() {
    uart_printf(CONSOLE, "[First Task]: Created Sender: %u\n\r", Create(9, &SenderTask));
    uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\n\r", Create(10, &ReceiverTask));
    Exit();
}

void RPSFirstUserTaskDebug() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(9, &NameServer));

    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\n\r", Create(9, &RPS_Server));

    uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client));

    uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client));

    Exit();
}

void RPSFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(9, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\n\r", Create(8, &RPS_Server));

    uart_printf(CONSOLE, "\n\r------  Starting first test: Both tie with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);
    char reply = 0;
    int player1 = Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    Send(player1, "F11", 3, &reply, 1);
    int player2 = Create(9, &RPS_Fixed_Client);
    Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting second test: Player 1 wins with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    Send(player1, "F11", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, Scissors, 1 iteration
    Send(player2, "F31", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting third test: Player 1 wins with Paper ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    Send(player1, "F21", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fourth test: Player 1 wins with Scissors ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    Send(player1, "F31", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    Send(player2, "F21", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fifth test: Random action choice for five rounds ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    Send(player1, "005", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    Send(player2, "005", 3, &reply, 1);

    uart_printf(CONSOLE,
                "\n\r------  Starting sixth test: Random action choice for three rounds, with three clients () "
                "---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Random_Client);

    player2 = Create(9, &RPS_Random_Client2);

    int player3 = Create(9, &RPS_Random_Client);

    uart_printf(CONSOLE, "\n\r------  Tests have completed! ---------\n\r");

    Exit();
}

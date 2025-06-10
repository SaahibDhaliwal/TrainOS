#include "user_tasks.h"

#include <string.h>

#include <cstdint>
#include <map>

#include "fixed_map.h"
#include "name_server.h"
#include "queue.h"
#include "rpi.h"
#include "rps_client.h"
#include "rps_server.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "timer.h"
#include "util.h"

void FirstUserTask() {
    //  Assumes FirstUserTask is at Priority 2
    const uint64_t lowerPrio = 2 - 1;
    const uint64_t higherPrio = 2 + 1;

    // create two tasks at a lower priority
    uart_printf(CONSOLE, "Created: %u\n\r", sys::Create(lowerPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", sys::Create(lowerPrio, &TestTask));

    // // create two tasks at a higher priority
    uart_printf(CONSOLE, "Created: %u\n\r", sys::Create(higherPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", sys::Create(higherPrio, &TestTask));

    uart_printf(CONSOLE, "FirstUserTask: exiting\n\r");
    sys::Exit();
}

void TestTask() {
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", sys::MyTid(), sys::MyParentTid());
    sys::Yield();
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", sys::MyTid(), sys::MyParentTid());
    sys::Exit();
}

void IdleTask() {
    uint32_t volatile sum_of_nonidle_ticks = 0;
    while (true) {
        asm volatile("wfi");
        asm volatile("mov %0, x0" : "=r"(sum_of_nonidle_ticks));
        uint32_t volatile get_tick = timerGetTick();
        uart_printf(CONSOLE, "[ Idle Task ] Fraction of execution ticks: %d / %d \n\r", get_tick - sum_of_nonidle_ticks,
                    get_tick);
    }
}

void SenderTask() {
    const char* msg =
        "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum "
        "sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies "
        "nec, pellentesque eu, pretium quis,.";

    int start = 0;
    int end = 0;

    char reply[Config::MAX_MESSAGE_LENGTH];
    start = timerGet();
    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        sys::Send(3, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    }
    end = timerGet();
    uart_printf(CONSOLE, "clock time diff %u \n\r", (end - start) / Config::EXPERIMENT_COUNT);

    sys::Exit();
}

void ReceiverTask() {
    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        uint32_t sender;
        char buffer[Config::MAX_MESSAGE_LENGTH];
        sys::Receive(&sender, buffer, Config::MAX_MESSAGE_LENGTH);
        sys::Reply(sender, "Got Message", Config::MAX_MESSAGE_LENGTH);
    }
    sys::Exit();
}

void PerformanceMeasurement() {
    uart_printf(CONSOLE, "[First Task]: Created Sender: %u\n\r", sys::Create(9, &SenderTask));
    uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\n\r", sys::Create(10, &ReceiverTask));
    sys::Exit();
}

void RPSFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", sys::Create(9, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\n\r", sys::Create(8, &RPS_Server));
    timerInit();

    uart_printf(CONSOLE, "\n\r------  Starting first test: Both tie with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);
    char reply = 0;
    int player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    sys::Send(player1, "F11", 3, &reply, 1);
    int player2 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting second test: Player 1 wins with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    sys::Send(player1, "F11", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Scissors, 1 iteration
    sys::Send(player2, "F31", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting third test: Player 1 wins with Paper ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    sys::Send(player1, "F21", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    sys::Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fourth test: Player 1 wins with Scissors ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    sys::Send(player1, "F31", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    sys::Send(player2, "F21", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fifth test: Random action choice for five rounds ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player1, "005", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player2, "005", 3, &reply, 1);

    uart_printf(CONSOLE,
                "\n\r------  Starting sixth test: Random action choice for three rounds, with three clients () "
                "---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    sys::Create(9, &RPS_Random_Client);
    sys::Create(9, &RPS_Random_Client2);
    sys::Create(9, &RPS_Random_Client);

    uart_printf(CONSOLE, "\n\r------  Tests have completed! ---------\n\r");

    sys::Exit();
}

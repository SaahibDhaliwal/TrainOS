#include "user_tasks.h"

#include <string.h>

#include <cstdint>

#include "clock_server.h"
#include "cursor.h"
#include "fixed_map.h"
#include "idle_time.h"
#include "marklin_server.h"
#include "name_server.h"
#include "protocols/ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "rps_client.h"
#include "rps_server.h"
#include "servers/console_server.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "timer.h"
#include "uptime.h"
#include "util.h"

// void FinalFirstUserTask() {
//     // do some initial print here:
//     cursor_top_left();
//     clear_screen();
//     cursor_top_left();
//     WITH_HIDDEN_CURSOR_BLOCKING(print_idle_percentage());
//     WITH_HIDDEN_CURSOR_BLOCKING(print_uptime());
//     uart_printf(CONSOLE, "\033[%d;%dH", 3, 0);

//     cursor_white();
//     show_cursor();
//     uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(49, &NameServer));
//     int console = sys::Create(30, &ConsoleServer);
//     uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", console);
//     int marklin_tid = sys::Create(31, &MarklinServer);
//     uart_printf(CONSOLE, "[First Task]: Created Marklin Server: %u\r\n", marklin_tid);
//     int clock = sys::Create(50, &ClockServer);
//     uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clock);

//     uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
//     // create notifiers
//     uint32_t clockNotifierTid = sys::Create(20, &PrinterClockNotifier);
//     uint32_t cmdNotifierTid = sys::Create(20, &PrinterCmdNotifier);

//     uint32_t percentage = 0;
//     int lastminute = 0;

//     sys::Exit();
// }

void FirstUserTask() {
    //  Assumes FirstUserTask is at Priority 2
    const uint64_t lowerPrio = 2 - 1;
    const uint64_t higherPrio = 2 + 1;

    // create two tasks at a lower priority
    uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(lowerPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(lowerPrio, &TestTask));

    // // create two tasks at a higher priority
    uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(higherPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(higherPrio, &TestTask));

    uart_printf(CONSOLE, "FirstUserTask: exiting\r\n");
    sys::Exit();
}

void TestTask() {
    uart_printf(CONSOLE, "[Task %u] Parent: %u\r\n", sys::MyTid(), sys::MyParentTid());
    sys::Yield();
    uart_printf(CONSOLE, "[Task %u] Parent: %u\r\n", sys::MyTid(), sys::MyParentTid());
    sys::Exit();
}

void IdleTask() {
    while (true) {
        asm volatile("wfi");
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
    uart_printf(CONSOLE, "clock time diff %u \r\n", (end - start) / Config::EXPERIMENT_COUNT);

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
    uart_printf(CONSOLE, "[First Task]: Created Sender: %u\r\n", sys::Create(9, &SenderTask));
    uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\r\n", sys::Create(10, &ReceiverTask));
    sys::Exit();
}

void RPSFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(9, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\r\n", sys::Create(8, &RPS_Server));
    timerInit();

    uart_printf(CONSOLE, "\r\n------  Starting first test: Both tie with Rock ---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);
    char reply = 0;
    int player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    sys::Send(player1, "F11", 3, &reply, 1);
    int player2 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\r\n------  Starting second test: Player 1 wins with Rock ---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    sys::Send(player1, "F11", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, Scissors, 1 iteration
    sys::Send(player2, "F31", 3, &reply, 1);

    uart_printf(CONSOLE, "\r\n------  Starting third test: Player 1 wins with Paper ---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    sys::Send(player1, "F21", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    sys::Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\r\n------  Starting fourth test: Player 1 wins with Scissors ---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    sys::Send(player1, "F31", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    sys::Send(player2, "F21", 3, &reply, 1);

    uart_printf(CONSOLE, "\r\n------  Starting fifth test: Random action choice for five rounds ---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);

    player1 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player1, "005", 3, &reply, 1);
    player2 = sys::Create(9, &RPS_Fixed_Client);
    sys::Send(player2, "005", 3, &reply, 1);

    uart_printf(CONSOLE,
                "\r\n------  Starting sixth test: Random action choice for three rounds, with three clients () "
                "---------\r\n");
    uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
    uart_getc(CONSOLE);

    sys::Create(9, &RPS_Random_Client);
    sys::Create(9, &RPS_Random_Client2);
    sys::Create(9, &RPS_Random_Client);

    uart_printf(CONSOLE, "\r\n------  Tests have completed! ---------\r\n");

    sys::Exit();
}

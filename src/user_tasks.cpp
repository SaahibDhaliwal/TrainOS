#include "user_tasks.h"

#include <stdint.h>
#include <string.h>

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "cursor.h"
#include "fixed_map.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "rps_client.h"
#include "rps_server.h"
#include "sensor.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "timer.h"
#include "turnout.h"
#include "uptime.h"
#include "util.h"

// step 1: printer proprietor
// step 2: command server who ges form command task
// don't need protocol for command servers, its job is to liteally parse the command
// step 3: marklin protocol that the command server uses
// reverse task
// step 4: clean up markin server like console server
// step 5:
// other tasks like sensor notifier

void CommandTask() {
    uint32_t printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);

    char userInput[256];
    int userInputIdx = 0;
    for (;;) {
        char ch = console_server::Getc(consoleTid, 0);

        if (ch >= 0) {
            // if (ch == 'q' || ch == 'Q') {
            //     return 0;
            // }

            // if (isInitializing) continue;

            if (ch == '\r') {
                print_clear_command_prompt(printerProprietorTid);
                userInput[userInputIdx] = '\0';  // mark end of user input command

                if (userInput[0] == 'd' && userInput[1] == 'u' && userInput[2] == 'm' && userInput[3] == 'p') {
                    int tid = name_server::WhoIs("console_dump");
                    emptySend(tid);
                }

                // if nothing was added to queue, it must have been an invalid command
                // int curQueueSize = queue_size(&marklinQueue);
                // process_input_command(userInput, &marklinQueue, trains, currenTimeMillis);
                // bool validCommand = curQueueSize != queue_size(&marklinQueue);
                // WITH_HIDDEN_CURSOR(&consoleQueue, print_command_feedback(&consoleQueue, validCommand));

                userInputIdx = 0;
            } else if (ch == '\b' || ch == 0x7F) {
                if (userInputIdx > 0) {
                    backspace(printerProprietorTid);
                    userInputIdx -= 1;
                }
            } else if (ch >= 0x20 && ch <= 0x7E && userInputIdx < 254) {
                userInput[userInputIdx++] = ch;
                printer_proprietor::printC(printerProprietorTid, 0, ch);
            }  // if
        }  // if
    }
}

void startup_print(int consoleTid) {
    hide_cursor(consoleTid);
    clear_screen(consoleTid);
    cursor_top_left(consoleTid);

    print_ascii_art(consoleTid);

    cursor_white(consoleTid);
    // uartPutS(consoleTid, "Press 'q' to reboot\n");
    print_uptime(consoleTid);
    print_idle_percentage(consoleTid);

    print_turnout_table(consoleTid);
    print_sensor_table(consoleTid);

    cursor_soft_pink(consoleTid);
    print_command_prompt_blocked(consoleTid);
    cursor_white(consoleTid);
}

void FinalFirstUserTask() {
    sys::Create(49, &NameServer);
    int clockTid = sys::Create(50, &ClockServer);

    int consoleTid = sys::Create(30, &ConsoleServer);
    int printerProprietorTid = sys::Create(49, &PrinterProprietor);
    startup_print(printerProprietorTid);

    uint32_t cmdNotifierTid = sys::Create(20, &CommandTask);

    sys::Exit();
}

// void FirstUserTask() {
//     //  Assumes FirstUserTask is at Priority 2
//     const uint64_t lowerPrio = 2 - 1;
//     const uint64_t higherPrio = 2 + 1;

//     // create two tasks at a lower priority
//     uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(lowerPrio, &TestTask));
//     uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(lowerPrio, &TestTask));

//     // // create two tasks at a higher priority
//     uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(higherPrio, &TestTask));
//     uart_printf(CONSOLE, "Created: %u\r\n", sys::Create(higherPrio, &TestTask));

//     uart_printf(CONSOLE, "FirstUserTask: exiting\r\n");
//     sys::Exit();
// }

// void TestTask() {
//     uart_printf(CONSOLE, "[Task %u] Parent: %u\r\n", sys::MyTid(), sys::MyParentTid());
//     sys::Yield();
//     uart_printf(CONSOLE, "[Task %u] Parent: %u\r\n", sys::MyTid(), sys::MyParentTid());
//     sys::Exit();
// }

void IdleTask() {
    while (true) {
        asm volatile("wfi");
    }
}

// void SenderTask() {
//     const char* msg =
//         "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa.
//         Cum " "sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis,
//         ultricies " "nec, pellentesque eu, pretium quis,.";

//     int start = 0;
//     int end = 0;

//     char reply[Config::MAX_MESSAGE_LENGTH];
//     start = timerGet();
//     for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
//         sys::Send(3, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
//     }
//     end = timerGet();
//     uart_printf(CONSOLE, "clock time diff %u \r\n", (end - start) / Config::EXPERIMENT_COUNT);

//     sys::Exit();
// }

// void ReceiverTask() {
//     for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
//         uint32_t sender;
//         char buffer[Config::MAX_MESSAGE_LENGTH];
//         sys::Receive(&sender, buffer, Config::MAX_MESSAGE_LENGTH);
//         sys::Reply(sender, "Got Message", Config::MAX_MESSAGE_LENGTH);
//     }
//     sys::Exit();
// }

// void PerformanceMeasurement() {
//     uart_printf(CONSOLE, "[First Task]: Created Sender: %u\r\n", sys::Create(9, &SenderTask));
//     uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\r\n", sys::Create(10, &ReceiverTask));
//     sys::Exit();
// }

// void RPSFirstUserTask() {
//     uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(9, &NameServer));
//     uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\r\n", sys::Create(8, &RPS_Server));
//     timerInit();

//     uart_printf(CONSOLE, "\r\n------  Starting first test: Both tie with Rock ---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);
//     char reply = 0;
//     int player1 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, Rock, 1 iteration
//     sys::Send(player1, "F11", 3, &reply, 1);
//     int player2 = sys::Create(9, &RPS_Fixed_Client);
//     sys::Send(player2, "F11", 3, &reply, 1);

//     uart_printf(CONSOLE, "\r\n------  Starting second test: Player 1 wins with Rock ---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);

//     player1 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, Rock, 1 iteration
//     sys::Send(player1, "F11", 3, &reply, 1);
//     player2 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, Scissors, 1 iteration
//     sys::Send(player2, "F31", 3, &reply, 1);

//     uart_printf(CONSOLE, "\r\n------  Starting third test: Player 1 wins with Paper ---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);

//     player1 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, paper, 1 iteration
//     sys::Send(player1, "F21", 3, &reply, 1);
//     player2 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, rock, 1 iteration
//     sys::Send(player2, "F11", 3, &reply, 1);

//     uart_printf(CONSOLE, "\r\n------  Starting fourth test: Player 1 wins with Scissors ---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);

//     player1 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, paper, 1 iteration
//     sys::Send(player1, "F31", 3, &reply, 1);
//     player2 = sys::Create(9, &RPS_Fixed_Client);
//     // Forced, rock, 1 iteration
//     sys::Send(player2, "F21", 3, &reply, 1);

//     uart_printf(CONSOLE, "\r\n------  Starting fifth test: Random action choice for five rounds ---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);

//     player1 = sys::Create(9, &RPS_Fixed_Client);
//     sys::Send(player1, "005", 3, &reply, 1);
//     player2 = sys::Create(9, &RPS_Fixed_Client);
//     sys::Send(player2, "005", 3, &reply, 1);

//     uart_printf(CONSOLE,
//                 "\r\n------  Starting sixth test: Random action choice for three rounds, with three clients () "
//                 "---------\r\n");
//     uart_printf(CONSOLE, "------  Press any key to start ---------\r\n");
//     uart_getc(CONSOLE);

//     sys::Create(9, &RPS_Random_Client);
//     sys::Create(9, &RPS_Random_Client2);
//     sys::Create(9, &RPS_Random_Client);

//     uart_printf(CONSOLE, "\r\n------  Tests have completed! ---------\r\n");

//     sys::Exit();
// }

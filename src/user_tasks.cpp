#include "user_tasks.h"

#include <stdint.h>
#include <string.h>

#include "clock_server.h"
#include "co_protocol.h"
#include "command.h"
#include "console_server.h"
#include "cs_protocol.h"
#include "cursor.h"
#include "fixed_map.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "marklin_server.h"
#include "ms_protocol.h"
#include "name_server.h"
#include "ns_protocol.h"
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

void startup_print(int consoleTid, Turnout* turnouts) {
    hide_cursor(consoleTid);
    clear_screen(consoleTid);
    cursor_top_left(consoleTid);

    print_ascii_art(consoleTid);

    cursor_white(consoleTid);
    // uartPutS(consoleTid, "Press 'q' to reboot\n");
    print_uptime(consoleTid);
    print_idle_percentage(consoleTid);

    print_turnout_table(turnouts, consoleTid);
    print_sensor_table(consoleTid);

    cursor_soft_pink(consoleTid);
    print_command_prompt_blocked(consoleTid);
    cursor_white(consoleTid);
}

void PrinterClockNotifier() {
    uint32_t clockTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    uint32_t printerTid = sys::MyParentTid();

    for (;;) {
        clock_server::Delay(clockTid, 10);
        char idleString[21] = {0};
        ui2a(sys::GetIdleTime(), 10, idleString);
        sys::Send(printerTid, idleString, strlen(idleString) + 1, nullptr, 0);
    }
}

void PrinterCmdNotifier() {
    uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    // uint32_t printerTid = sys::MyParentTid();

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
                print_clear_command_prompt(consoleTid);
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
                    backspace(consoleTid);
                    userInputIdx -= 1;
                }
            } else if (ch >= 0x20 && ch <= 0x7E && userInputIdx < 254) {
                userInput[userInputIdx++] = ch;
                console_server::Putc(consoleTid, 0, ch);
            }  // if
        }  // if
    }
}

void FinalFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(49, &NameServer));

    int clockTid = sys::Create(50, &ClockServer);
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clockTid);

    int consoleTid = sys::Create(30, &ConsoleServer);
    uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", consoleTid);

    int marklinTid = sys::Create(31, &MarklinServer);
    uart_printf(CONSOLE, "[First Task]: Created Marklin Server: %u\r\n", marklinTid);

    // uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts
    // initialize_turnouts(turnouts, &marklinQueue);

    startup_print(consoleTid, turnouts);  // initial display
                                          // create notifiers

    uint32_t clockNotifierTid = sys::Create(20, &PrinterClockNotifier);
    uint32_t cmdNotifierTid = sys::Create(20, &PrinterCmdNotifier);

    uint32_t percentage = 0;
    int lastminute = 0;

    if (clock_server::Delay(clockTid, 10) == -1) {
        uart_printf(CONSOLE, "DELAY Cannot reach TID\r\n");
    }

    for (;;) {
        uint32_t clientTid;
        char msg[22] = {0};  // change this
        int msgLen = sys::Receive(&clientTid, msg, 22);

        if (clientTid == clockNotifierTid) {
            update_uptime(consoleTid, timerGet());

            a2ui(msg, 10, &percentage);
            update_idle_percentage(percentage, consoleTid);

            emptyReply(clockNotifierTid);

        } else if (clientTid == cmdNotifierTid) {
            if (msg[0] == 'S') {
                // clock_server::Delay(clock, 100);
                // uartPutS(consoleTid, "DelayDone");
                marklin_server::Putc(marklinTid, 0, 16);  // speed STOP W LIGHTS
                marklin_server::Putc(marklinTid, 0, 15);  // train 14
            } else if (msg[0] == 'G') {
                // clock_server::Delay(clock, 100);
                // uartPutS(consoleTid, "DelayDone");
                marklin_server::Putc(marklinTid, 0, 16 + 7);  // speed 7 WITH LIGHTS
                marklin_server::Putc(marklinTid, 0, 15);      // train 14
            } else if (msg[0] == 's') {
                // clock_server::Delay(clock, 100);
                // uartPutS(consoleTid, "DelayDone");
                marklin_server::Putc(marklinTid, 0, 0);   // STOP W NO LIGHTS
                marklin_server::Putc(marklinTid, 0, 15);  // train 14
            }

            console_server::Putc(consoleTid, 0, msg[0]);
            emptyReply(cmdNotifierTid);
        }
    }

    sys::Exit();
}

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

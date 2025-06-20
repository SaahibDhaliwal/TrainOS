#include "printer_server.h"

#include "clock_server.h"
#include "co_protocol.h"
#include "config.h"
#include "console_server.h"
#include "cs_protocol.h"
#include "cursor.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "marklin_server.h"
#include "ms_protocol.h"
#include "name_server.h"
#include "ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

// using namespace print_server;

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
    uint32_t printerTid = sys::MyParentTid();
    char msg[2] = {0};
    msg[1] = '\0';
    int c = 0;
    for (;;) {
        c = console_server::Getc(consoleTid, 0);
        if (c < 0) {
            uart_printf(CONSOLE, "GETC Cannot reach TID\r\n");
        }
        msg[0] = c;
        sys::Send(printerTid, msg, 2, nullptr, 0);
    }
}

void timerHelper(uint32_t consoleTid, int* lastminute) {
    unsigned int t = timerGet() / 1000;
    int allseconds = t / 1000;
    int minutes = allseconds / 60;
    int seconds = allseconds % 60;
    // char* outputStr = nullptr;
    // outputStr = "\033[s";

    // outputStr = "\033[?25l";
    // if (minutes != *lastminute) {
    *lastminute = minutes;
    char buff[2];
    ui2a(minutes, 10, buff);
    uartPutConsoleS(consoleTid, "\033[2;11H");
    if (minutes < 10) {
        uartPutConsoleS(consoleTid, "0");
    }
    // if (minutes < 10) {
    //     uartPutConsoleS(consoleTid, "\033[2;11H0");  // can replace with printf
    //     uartPutConsoleS(consoleTid, "\033[2;12H");
    // } else {
    //     uartPutConsoleS(consoleTid, "\033[2;11H");
    // }
    uartPutConsoleS(consoleTid, buff);
    // }

    char buff2[2];
    ui2a(seconds, 10, buff2);
    // if (seconds < 10) {
    //     uartPutConsoleS(consoleTid, "\033[2;17H0");  // can replace with printf
    //     uartPutConsoleS(consoleTid, "\033[2;18H");
    // } else {
    //     uartPutConsoleS(consoleTid, "\033[2;17H");
    // }
    uartPutConsoleS(consoleTid, " m  ");
    if (seconds < 10) {
        uartPutConsoleS(consoleTid, "0");
    }
    uartPutConsoleS(consoleTid, buff2);
    uartPutConsoleS(consoleTid, ".");

    t = t / 100;
    // uartPutConsoleS(consoleTid, "\033[2;20H");
    char buff3[1];
    ui2a(t % 10, 10, buff3);
    uartPutConsoleS(consoleTid, buff3);
    uartPutConsoleS(consoleTid, " s");

    // uartPutConsoleS(printTid, "\033[?25h");  // show cursor
    // uartPutConsoleS(printTid, "\033[u");
}

void PrinterServer() {
    // do some initial print here:
    cursor_top_left();
    clear_screen();
    cursor_top_left();
    WITH_HIDDEN_CURSOR(print_idle_percentage());
    uart_printf(CONSOLE, "\r\n");
    // 	ansi("2;1H", q);
    // string2q(CONSOLE, " Uptime: 00 m  00.0 s", q);

    uart_printf(CONSOLE, "  Uptime: 00 m  00.0 s");
    uart_printf(CONSOLE, "\r\n");
    cursor_white();
    show_cursor();
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(49, &NameServer));
    int console = sys::Create(30, &ConsoleServer);
    uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", console);
    int marklin_tid = sys::Create(31, &MarklinServer);
    uart_printf(CONSOLE, "[First Task]: Created Marklin Server: %u\r\n", marklin_tid);
    int clock = sys::Create(50, &ClockServer);
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clock);

    int registerReturn = name_server::RegisterAs(PRINTER_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER\r\n");
        sys::Exit();
    }

    uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    // create notifiers
    uint32_t clockNotifierTid = sys::Create(20, &PrinterClockNotifier);
    uint32_t cmdNotifierTid = sys::Create(20, &PrinterCmdNotifier);

    uint32_t percentage = 0;
    int lastminute = 0;

    if (clock_server::Delay(clock, 10) == -1) {
        uart_printf(CONSOLE, "DELAY Cannot reach TID\r\n");
    }

    for (;;) {
        uint32_t clientTid;
        char msg[22] = {0};  // change this
        int msgLen = sys::Receive(&clientTid, msg, 22);

        if (clientTid == clockNotifierTid) {
            // update time since boot AND idle time percentage
            uartPutConsoleS(consoleTid, "\x1b[s");
            // uartPutConsoleS(consoleTid, "\033[?25l");
            timerHelper(consoleTid, &lastminute);
            a2ui(msg, 10, &percentage);  // turn msg into an int
            update_idle_percentage(percentage, consoleTid);
            // uartPutConsoleS(consoleTid, "\033[?25h");
            uartPutConsoleS(consoleTid, "\x1b[u");  // reminder: try this change before uncommenting the percentage
            emptyReply(clockNotifierTid);

        } else if (clientTid == cmdNotifierTid) {
            if (msg[0] == 'S') {
                // clock_server::Delay(clock, 100);
                // uartPutConsoleS(consoleTid, "DelayDone");
                marklin_server::Putc(marklin_tid, 0, 16);  // speed STOP W LIGHTS
                marklin_server::Putc(marklin_tid, 0, 15);  // train 14
            } else if (msg[0] == 'G') {
                // clock_server::Delay(clock, 100);
                // uartPutConsoleS(consoleTid, "DelayDone");
                marklin_server::Putc(marklin_tid, 0, 16 + 7);  // speed 7 WITH LIGHTS
                marklin_server::Putc(marklin_tid, 0, 15);      // train 14
            } else if (msg[0] == 's') {
                // clock_server::Delay(clock, 100);
                // uartPutConsoleS(consoleTid, "DelayDone");
                marklin_server::Putc(marklin_tid, 0, 0);   // STOP W NO LIGHTS
                marklin_server::Putc(marklin_tid, 0, 15);  // train 14
            }
            console_server::Putc(consoleTid, 0, msg[0]);
            emptyReply(cmdNotifierTid);
        }
    }
}
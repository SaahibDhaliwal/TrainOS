
#include "console_server.h"

#include "co_protocol.h"
#include "config.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "name_server.h"
#include "ns_protocol.h"
#include "queue.h"
// #include "rpi.h"
#include "protocols/cs_protocol.h"
#include "ring_buffer.h"
#include "servers/clock_server.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

using namespace console_server;

void ConsoleRXNotifier() {
    int consoleServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::CONSOLE_RXRT);

    charSend(consoleServerTid, toByte(Command::RX_CONNECT));
    for (;;) {
        sys::AwaitEvent(eventChoice);
        charSend(consoleServerTid, toByte(Command::RX));
    }
}

void ConsoleTXNotifier() {
    int consoleServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::CONSOLE_TX);

    charSend(consoleServerTid, toByte(Command::TX_CONNECT));
    for (;;) {
        sys::AwaitEvent(eventChoice);
        charSend(consoleServerTid, toByte(Command::TX));
    }
}

void ConsoleServer() {
    int registerReturn = name_server::RegisterAs(CONSOLE_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    RingBuffer<char, Config::CONSOLE_QUEUE_SIZE> charQueue;

    int notifierPriority = 63;  // TODO: think about this prio

    uint32_t txNotifier = sys::Create(notifierPriority, ConsoleTXNotifier);
    uint32_t rxNotifier = sys::Create(notifierPriority, ConsoleRXNotifier);

    uint32_t rxClientTid = -1;  // assumption is one command task

    for (;;) {
        uint32_t clientTid;
        char msg[2] = {0};
        int msgLen = sys::Receive(&clientTid, msg, 2);

        Command command = commandFromByte(msg[0]);

        switch (command) {
            case Command::TX_CONNECT:
            case Command::RX_CONNECT: {  // initial notifier connection (TX or RX)
                break;
            }
            case Command::TX: {
                ASSERT(!charQueue.empty(), "TX WAS ENABLED WHEN WE HAD NO CHARS TO PROCESS");
                ASSERT(!uartTXFull(CONSOLE), "TX SUPPOSED TO BE FREE");

                do {
                    uartPutTX(CONSOLE, *charQueue.pop());
                } while (!charQueue.empty() && !uartTXFull(CONSOLE));  // drain as much as we can into the TX buffer

                if (!charQueue.empty()) {
                    emptyReply(txNotifier);  // re-enable TX interrupts
                }

                break;
            }
            case Command::RX: {
                ASSERT(rxClientTid != -1, "RX WAS ENABLED WHEN WE HAD NO WAITING CLIENT");
                ASSERT(!uartRXEmpty(CONSOLE), "RX SUPPOSED TO HAVE DATA");

                charReply(rxClientTid, uartGetRX(CONSOLE));  // unblock client
                rxClientTid = -1;
                break;
            }
            case Command::PUT: {
                if (!uartTXFull(CONSOLE) && charQueue.empty()) {
                    uartPutTX(CONSOLE, msg[1]);
                } else {
                    ASSERT(!charQueue.full(), "QUEUE LIMIT HIT");
                    charQueue.push(msg[1]);

                    if (charQueue.size() == 1) {  // first pending char will enable TX interrupt
                        emptyReply(txNotifier);
                    }
                }

                charReply(clientTid, toByte(Reply::SUCCESS));  // unblock client
                break;
            }
            case Command::GET: {
                rxClientTid = clientTid;
                if (!uartRXEmpty(CONSOLE)) {  // already have something to give?
                    charReply(rxClientTid, uartGetRX(CONSOLE));
                } else {
                    emptyReply(rxNotifier);  // enable RX interrupt
                }

                break;
            }
            default: {
                ASSERT(0, "INVALID COMMAND SENT TO CONSOLE SERVER");
            }
        }
    }
}

void ConsoleFirstUserTask() {
    int ctid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    uart_printf(CONSOLE, "Done WHOIS\r\n");

    cursor_top_left(ctid);
    clear_screen(ctid);
    cursor_top_left(ctid);
    WITH_HIDDEN_CURSOR(ctid, print_idle_percentage(ctid));
    uart_printf(CONSOLE, "\r\r\n\n");
    cursor_white(ctid);

    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(49, &NameServer));
    int clock = sys::Create(50, &ClockServer);
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clock);
    uart_printf(CONSOLE, "[First Task]: Attempting to create ConsoleServer...\r\n");
    int console = sys::Create(49, &ConsoleServer);
    uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", console);

    int response = 0;

    for (;;) {
        int c = console_server::Getc(ctid, 0);
        if (c < 0) {
            uart_printf(CONSOLE, "GETC Cannot reach TID\r\n");
        }
        console_server::Putc(ctid, 0, c);
        // uart_printf(CONSOLE, "%c\n\r", c);
    }

    sys::Exit();
}


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
#include "servers/clock_server.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

using namespace console_server;

void ConsoleNotifier() {
    // int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    int consoleServerTid = sys::MyParentTid();
    // emptySend(consoleServerTid);
    uart_printf(CONSOLE, "reached notifier \n\r");
    for (;;) {
        int interruptBits = sys::AwaitEvent(static_cast<int>(EVENT_ID::TERMINAL));
        uart_printf(CONSOLE, "notifier MIS value: %d", interruptBits);

        ASSERT(interruptBits > 1);
        // check what interrupt it is. At most, we have two sends back to back
        if (interruptBits >= static_cast<int>(MIS::RT)) {
            // We don't care if the receive buffer is full or hit... yet
            // sys::Send(consoleServerTid, "RT", 3, nullptr, 0);
            interruptBits -= static_cast<int>(MIS::RT);
        }
        if (interruptBits >= static_cast<int>(MIS::TX)) {
            const char sendBuf = toByte(Command::TX);
            sys::Send(consoleServerTid, &sendBuf, 1, nullptr, 0);
            interruptBits -= static_cast<int>(MIS::TX);
        }
        if (interruptBits >= static_cast<int>(MIS::RX)) {
            const char sendBuf = toByte(Command::RX);
            sys::Send(consoleServerTid, &sendBuf, 1, nullptr, 0);
            interruptBits -= static_cast<int>(MIS::RX);
        }
    }
}

class CharNode : public IntrusiveNode<CharNode> {
   public:
    char c;
};

void ConsoleServer() {
    int registerReturn = name_server::RegisterAs(CONSOLE_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER\r\n");
        sys::Exit();
    }
    // initialize character queue and client queue
    Queue<CharNode> characterQueue;

    CharNode consoleClientSlabs[Config::CONSOLE_PRINT_QUEUE];
    Stack<CharNode> freelist;

    for (int i = 0; i < Config::CONSOLE_PRINT_QUEUE; i += 1) {
        freelist.push(&consoleClientSlabs[i]);
    }

    // create notifiers
    int notifierPriority = 60;  // should be higher priority than ConsoleServer
    // otherwise, we may go to receive and then try to reply to our notifier when it wasn't waiting for a reply?
    uint32_t notifierTid = sys::Create(notifierPriority, ConsoleNotifier);

    // uart_printf(CONSOLE, "notifierTID: %u", notifierTid);
    uint32_t cmdUserTid = 0;
    // bool notifiedFlag = true;

    for (;;) {
        uint32_t clientTid;
        char msg[2] = {0};
        int msgLen = sys::Receive(&clientTid, msg, 2);

        // uart_printf(CONSOLE, "Got msg\r\n");

        Command command = commandFromByte(msg[0]);

        if (clientTid == notifierTid) {
            if (command == Command::TX && !characterQueue.empty()) {
                // pop from queue and print
                CharNode* next_char = characterQueue.pop();
                uartPutTX(CONSOLE, next_char->c);
                freelist.push(next_char);
                // note that we DO NOT reply to the notifier until we want another interrupt to happen

            } else if (command == Command::RX && cmdUserTid != 0) {  // if someone actually wants it
                charReply(cmdUserTid, uartGetRX(CONSOLE));           // reply to previous client
            } else {
                uart_printf(CONSOLE, "Something broke between the console server & notifier...\n\r");
            }
            // notifiedFlag = false;
        } else if (command == Command::PUT) {
            // uart_printf(CONSOLE, "Got char: %c", msg[1]);
            if (uartCheckTX(CONSOLE)) {
                uartPutTX(CONSOLE, msg[1]);
            } else {
                // pop from free list and add to queue
                CharNode* incoming_char = freelist.pop();
                incoming_char->c = msg[1];
                characterQueue.push(incoming_char);
                // reply to notifier to signal we want interrupts enabled
                // if (!notifiedFlag) {
                // charReply(notifierTid, '0');
                emptyReply(notifierTid);
                //     notifiedFlag = true;
                // }
            }
            // reply to client regardless
            charReply(clientTid, static_cast<char>(Reply::SUCCESS));

        } else if (command == Command::GET) {
            // there should only ever be one person asking for getc
            cmdUserTid = clientTid;
            if (uartCheckRX(CONSOLE)) {
                charReply(cmdUserTid, uartGetRX(CONSOLE));
            } else {
                // reply to notifier to signal we want interrupts enabled
                // if (!notifiedFlag) {
                // charReply(notifierTid, '0');
                emptyReply(notifierTid);
                //     notifiedFlag = true;
                // }
            }
            // DO NOT REPLY TO CLIENT HERE

        } else {
            uart_printf(CONSOLE, "Something broke in the console server...\n\r");
        }
    }
}

void ConsoleFirstUserTask() {
    cursor_top_left();
    clear_screen();
    cursor_top_left();
    WITH_HIDDEN_CURSOR(print_idle_percentage());
    uart_printf(CONSOLE, "\r\r\n\n");
    cursor_white();

    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\r\n", sys::Create(49, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Attempting to create ConsoleServer...\r\n");
    int console = sys::Create(49, &ConsoleServer);
    uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", console);
    int clock = sys::Create(50, &ClockServer);
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clock);
    int ctid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    uart_printf(CONSOLE, "Done WHOIS\r\n");
    int response = 0;
    for (;;) {
        if (clock_server::Delay(clock, 2) == -1) {
            uart_printf(CONSOLE, "DELAY Cannot reach TID\r\n");
        }
        response = console_server::Putc(ctid, 0, 'a');
        if (response) {
            uart_printf(CONSOLE, "PUTC Cannot reach TID\r\n");
        }
    }

    // for (;;) {
    //     const char c = Getc(ctid, 0);
    //     uart_printf(CONSOLE, "%c", c);
    // }

    sys::Exit();
}

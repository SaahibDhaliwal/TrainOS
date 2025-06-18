
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
    for (;;) {
        int interruptBits = sys::AwaitEvent(static_cast<int64_t>(EVENT_ID::TERMINAL));
        // uart_printf(CONSOLE, "notifier MIS value: %d\n\r", interruptBits);

        ASSERT(interruptBits > 1);
        // check what interrupt it is. At most, we have two sends back to back
        if (interruptBits >= static_cast<int>(MIS::RT)) {
            // We DO care if theres a receive timeout
            const char sendBuf = toByte(Command::RT);
            sys::Send(consoleServerTid, &sendBuf, 1, nullptr, 0);
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
    bool notifiedFlag = true;

    for (;;) {
        uint32_t clientTid;
        char msg[2] = {0};
        int msgLen = sys::Receive(&clientTid, msg, 2);

        Command command = commandFromByte(msg[0]);

        if (clientTid == notifierTid) {
            if (command == Command::TX && !characterQueue.empty()) {
                // pop from queue and print
                CharNode* next_char = characterQueue.pop();
                uartPutTX(CONSOLE, next_char->c);
                freelist.push(next_char);
                // note that we DO NOT reply to the notifier until we want another interrupt to happen
                // so we should check our queue and reply anyways, since we WANT another interrupt to happen so we can
                // pop our queue

                if (!characterQueue.empty()) {
                    // let the notifier know we want another interrupt since our queue isnt empty
                    emptyReply(notifierTid);
                } else {
                    // it is empty, so we can hold off on replying until we want another interrupt
                    notifiedFlag = false;
                }

            } else if (cmdUserTid != 0) {  // if someone actually wants it
                if (command == Command::RX || command == Command::RT) {
                    charReply(cmdUserTid, uartGetRX(CONSOLE));  // reply to previous client
                    cmdUserTid = 0;        // so that we don't get here again until we have someone asking for a getc
                    notifiedFlag = false;  // hold off on replying to notifier until next time
                } else {
                    emptyReply(notifierTid);  // we still want a receive
                }

            } else if (cmdUserTid == 0) {
                uart_printf(CONSOLE, "nobody waiting on receive rn\n\r");
            } else {
                uart_printf(CONSOLE, "Notifier did not send a valid command\n\r");
            }

        } else if (command == Command::PUT) {
            // uart_printf(CONSOLE, "Got msg: TX: %c\r\n", msg[1]);
            if (uartCheckTX(CONSOLE)) {
                uartPutTX(CONSOLE, msg[1]);
            } else {
                // pop from free list and add to queue
                CharNode* incoming_char = freelist.pop();
                incoming_char->c = msg[1];
                characterQueue.push(incoming_char);
                // reply to notifier to signal we want interrupts enabled (if it's not notified already)
                if (!notifiedFlag) {
                    uart_printf(CONSOLE, "Replying to notifier...\r\n");
                    emptyReply(notifierTid);
                    notifiedFlag = true;
                }
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
                // uart_printf(CONSOLE, "Nothing in RX\r\n");
                if (!notifiedFlag) {
                    // uart_printf(CONSOLE, "Replying to notifier...\r\n");
                    emptyReply(notifierTid);
                    notifiedFlag = true;
                }
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
    // if (clock_server::Delay(clock, 10) == -1) {
    //     uart_printf(CONSOLE, "DELAY Cannot reach TID\r\n");
    // }
    // for (int i = 0; i < 100; i++) {
    //     response = console_server::Putc(ctid, 0, 'a');
    //     if (response) {
    //         uart_printf(CONSOLE, "PUTC Cannot reach TID\r\n");
    //     }
    // }

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

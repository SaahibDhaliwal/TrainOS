
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

// issue: the notifier will keep notifying about TX being available, even though we are waiting for RX.
//  Ideally, we would disable TX if we didn't need it and only enable RX.
//  But if we are in awaitEvent() for RX and then the server wants to transmit with TX, it will reply to the notifier
//  who did not send
void ConsoleRXNotifier() {
    int consoleServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::CONSOLE_RXRT);

    int txCompare = static_cast<int>(MIS::TX);
    int rxCompare = static_cast<int>(MIS::RX);
    int rtCompare = static_cast<int>(MIS::RT);

    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits > 1);
        if (interruptBits >= rtCompare || (interruptBits >= rxCompare && interruptBits < txCompare)) {
            // We DO care if theres a receive timeout or RX nonempty
            const char sendBuf = toByte(Command::RX);
            sys::Send(consoleServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            uart_printf(CONSOLE, "Error in RX notifier\n\r");
        }
    }
}

void ConsoleTXNotifier() {
    // int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    int consoleServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::CONSOLE_TX);

    int txCompare = static_cast<int>(MIS::TX);
    int rtCompare = static_cast<int>(MIS::RT);

    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits > 1);
        if (interruptBits >= txCompare && interruptBits <= rtCompare) {
            const char sendBuf = toByte(Command::TX);
            sys::Send(consoleServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            // we get here if we got back from waiting on a TX event, but the MIS TX bit was not 1
            uart_printf(CONSOLE, "Error in TX notifier\n\r");
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
    int notifierPriority = 32;  // should be higher priority than ConsoleServer (I think)
    // otherwise, we may go to receive and then try to reply to our notifier when it wasn't waiting for a reply?
    // update: this might not be the case anymore after the refactor
    uint32_t txNotifier = sys::Create(notifierPriority, ConsoleTXNotifier);
    uint32_t rxNotifier = sys::Create(notifierPriority, ConsoleRXNotifier);

    // uart_printf(CONSOLE, "notifierTID: %u", notifierTid);
    uint32_t cmdUserTid = 0;
    bool txNotifierFlag = true;
    bool rxNotifierFlag = true;

    // const char requestAllChar = static_cast<char>(EVENT_ID::CONSOLE_ALL);
    // const char requestTXChar = static_cast<char>(EVENT_ID::CONSOLE_TX);
    // const char requestRXRTChar = static_cast<char>(EVENT_ID::CONSOLE_RXRT);

    for (;;) {
        uint32_t clientTid;
        char msg[2] = {0};
        int msgLen = sys::Receive(&clientTid, msg, 2);

        Command command = commandFromByte(msg[0]);

        if (clientTid == txNotifier) {
            if (command == Command::TX && !characterQueue.empty()) {
                // pop from queue and print
                CharNode* next_char = characterQueue.pop();
                if (!uartCheckTX(CONSOLE)) {
                    uart_printf(CONSOLE, "Notifier said it was free, but it wasn't\n\r");
                }
                uartPutTX(CONSOLE, next_char->c);
                freelist.push(next_char);
                // note that we DO NOT reply to the notifier until we want another interrupt to happen
                // so we should check our queue and reply anyways, since we WANT another interrupt to happen so we can
                // pop our queue

                if (!characterQueue.empty()) {
                    // let the notifier know we want another interrupt since our queue isnt empty
                    emptyReply(txNotifier);
                } else {
                    // it is empty, so we can hold off on replying until we want another interrupt
                    txNotifierFlag = false;
                }

            } else if (command != Command::TX) {
                uart_printf(CONSOLE, "Notifier did not send a valid command\n\r");
            } else {
                // otherwise, the character queue is empty
                //  why would we get here after the first loop? we should not
                txNotifierFlag = false;
            }

        } else if (clientTid == rxNotifier) {
            if (cmdUserTid != 0) {  // if someone actually wants it
                if (command == Command::RX) {
                    charReply(cmdUserTid, uartGetRX(CONSOLE));  // reply to previous client
                    cmdUserTid = 0;          // so that we don't get here again until we have someone asking for a getc
                    rxNotifierFlag = false;  // hold off on replying to notifier until next time
                } else {
                    uart_printf(CONSOLE, "Notifier did not send a valid command\n\r");
                }
            } else {
                // we should never get here after the first loop
                // uart_printf(CONSOLE, "Nobody waiting on receive\n\r");
                rxNotifierFlag = false;
            }

        } else if (command == Command::PUT) {
            if (uartCheckTX(CONSOLE) && !txNotifierFlag) {
                uartPutTX(CONSOLE, msg[1]);
            } else {
                // pop from free list and add to queue
                CharNode* incoming_char = freelist.pop();
                incoming_char->c = msg[1];
                characterQueue.push(incoming_char);
                if (characterQueue.size() == Config::CONSOLE_PRINT_QUEUE) {
                    uart_printf(CONSOLE, "QUEUE LIMIT HIT!!!!!!!!!!!!\n\r");
                }
                // reply to notifier to signal we want interrupts enabled (if it's not notified already)
                if (!txNotifierFlag) {
                    // uart_printf(CONSOLE, "Replying to notifier...\r\n");
                    emptyReply(txNotifier);
                    txNotifierFlag = true;
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
                if (!rxNotifierFlag) {
                    emptyReply(rxNotifier);
                    rxNotifierFlag = true;
                }
            }
            // DO NOT REPLY TO CLIENT HERE. GETC MUST BE BLOCKING

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
    int clock = sys::Create(50, &ClockServer);
    uart_printf(CONSOLE, "[First Task]: Created Clock Server: %u\r\n", clock);
    uart_printf(CONSOLE, "[First Task]: Attempting to create ConsoleServer...\r\n");
    int console = sys::Create(49, &ConsoleServer);
    uart_printf(CONSOLE, "[First Task]: Console server created! TID: %u\r\n", console);

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

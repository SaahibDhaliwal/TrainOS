#include "marklin_server.h"

#include "co_protocol.h"
#include "config.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "ms_protocol.h"
#include "name_server.h"
#include "ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

using namespace marklin_server;

typedef enum {
    GOOD_TO_TRANSMIT,
    WAITING_ON_BOX,
    SLOW_BOX,  // Tx high, CTS high, but CTS did not go low
    BOTH_SLOW,
    BOX_PROCESSING,
} state_t;

typedef enum {
    TX_LOW,
    TX_HIGH,
    CTS_LOW,
    CTS_HIGH,
} interrupt_t;

class StateMachine {
   private:
    state_t currentState;
    interrupt_t txStatus;
    interrupt_t ctsStatus;

   public:
    StateMachine();
    bool checkState();
    bool changeState(interrupt_t move);
};

StateMachine::StateMachine()
    : currentState(state_t::GOOD_TO_TRANSMIT), txStatus(interrupt_t::TX_HIGH), ctsStatus(interrupt_t::CTS_HIGH) {
}

bool StateMachine::checkState() {
    if (currentState == GOOD_TO_TRANSMIT) {
        return true;
    }
}

bool StateMachine::changeState(interrupt_t move) {
    switch (currentState) {
        case GOOD_TO_TRANSMIT: {
            if (move == TX_LOW) {  // make sure to transmit this after we check the state and transmit
                currentState = WAITING_ON_BOX;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: GOOD_TO_TRANSMIT and move is not TX_LOW");
            }
        }
        case WAITING_ON_BOX: {
            switch (move) {
                case CTS_LOW:
                    currentState = BOTH_SLOW;
                    break;
                case TX_HIGH:
                    currentState = SLOW_BOX;
                default:
                    uart_printf(CONSOLE, "Bad state change: WAITING_ON_BOX with incorrect move");
                    break;
            }
            break;
        }
        case BOTH_SLOW: {
            if (move == TX_HIGH) {
                currentState = BOX_PROCESSING;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: BOTH_SLOW and move is not TX_HIGH");
            }
        }
        case SLOW_BOX: {
            if (move == CTS_LOW) {
                currentState = BOX_PROCESSING;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: SLOW_BOX and move is not CTS_LOW");
            }
        }
        case BOX_PROCESSING: {
            if (move == CTS_HIGH) {
                currentState = GOOD_TO_TRANSMIT;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: BOX_PROCESSING and move is not CTS_HIGH");
            }
        }
    }
}

void MarklinRXNotifier() {
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_RX);

    int txCompare = static_cast<int>(MIS::TX);
    int rxCompare = static_cast<int>(MIS::RX);
    int rtCompare = static_cast<int>(MIS::RT);

    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits > 1);
        if (interruptBits >= rtCompare || (interruptBits >= rxCompare && interruptBits < txCompare)) {
            // We DO care if theres a receive timeout or RX nonempty
            const char sendBuf = toByte(Command::RX);
            sys::Send(marklinServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            uart_printf(CONSOLE, "Error in RX notifier\n\r");
        }
    }
}

void MarklinTXNotifier() {
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_TX);

    int txCompare = static_cast<int>(MIS::TX);
    int rtCompare = static_cast<int>(MIS::RT);

    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits > 1);
        if (interruptBits >= txCompare && interruptBits <= rtCompare) {
            const char sendBuf = toByte(Command::TX);
            sys::Send(marklinServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            // we get here if we got back from waiting on a TX event, but the MIS TX bit was not 1
            uart_printf(CONSOLE, "Error in TX notifier\n\r");
        }
    }
}

void MarklinCTSNotifier() {
    // int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_CTS);

    int txCompare = static_cast<int>(MIS::TX);
    int rtCompare = static_cast<int>(MIS::RT);

    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits >= 1);
        if (interruptBits < txCompare && interruptBits <= rtCompare) {
            const char sendBuf = toByte(Command::TX);
            sys::Send(marklinServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            // we get here if we got back from waiting on a TX event, but the MIS TX bit was not 1
            uart_printf(CONSOLE, "Error in CTS notifier\n\r");
        }
    }
}

class CharNode : public IntrusiveNode<CharNode> {
   public:
    char c;
};

void MarklinServer() {
    int registerReturn = name_server::RegisterAs(MARKLIN_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER\r\n");
        sys::Exit();
    }
    // initialize character queue and client queue
    Queue<CharNode> characterQueue;

    CharNode marklinClientSlabs[Config::MARKLIN_PRINT_QUEUE];
    Stack<CharNode> freelist;

    for (int i = 0; i < Config::MARKLIN_PRINT_QUEUE; i += 1) {
        freelist.push(&marklinClientSlabs[i]);
    }

    // create notifiers
    int notifierPriority = 60;  // should be higher priority than ConsoleServer (I think)
    // otherwise, we may go to receive and then try to reply to our notifier when it wasn't waiting for a reply?
    // update: this might not be the case anymore after the refactor
    uint32_t txNotifier = sys::Create(notifierPriority, MarklinTXNotifier);
    uint32_t rxNotifier = sys::Create(notifierPriority, MarklinRXNotifier);
    uint32_t ctsNotifier = sys::Create(notifierPriority, MarklinCTSNotifier);

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
                if (!uartCheckTX(MARKLIN)) {
                    uart_printf(CONSOLE, "Notifier said it was free, but it wasn't\n\r");
                }
                uartPutTX(MARKLIN, next_char->c);
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
                    charReply(cmdUserTid, uartGetRX(MARKLIN));  // reply to previous client
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
            if (uartCheckTX(MARKLIN) && !txNotifierFlag) {
                uartPutTX(MARKLIN, msg[1]);
            } else {
                // pop from free list and add to queue
                CharNode* incoming_char = freelist.pop();
                incoming_char->c = msg[1];
                characterQueue.push(incoming_char);
                if (characterQueue.size() == Config::MARKLIN_PRINT_QUEUE) {
                    uart_printf(CONSOLE, "QUEUE LIMIT HIT!!\n\r");
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
            if (uartCheckRX(MARKLIN)) {
                charReply(cmdUserTid, uartGetRX(MARKLIN));
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

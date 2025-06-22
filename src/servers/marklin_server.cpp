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
    void changeState(interrupt_t move);
};

StateMachine::StateMachine()
    : currentState(state_t::GOOD_TO_TRANSMIT), txStatus(interrupt_t::TX_HIGH), ctsStatus(interrupt_t::CTS_HIGH) {
}

bool StateMachine::checkState() {
    if (currentState == GOOD_TO_TRANSMIT) {
        return true;
    }
    return false;
}

void StateMachine::changeState(interrupt_t move) {
    switch (currentState) {
        case GOOD_TO_TRANSMIT: {
            if (move == TX_LOW) {  // make sure to transmit this after we check the state and transmit
                currentState = WAITING_ON_BOX;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: GOOD_TO_TRANSMIT and move is not TX_LOW");
            }
            break;
        }
        case WAITING_ON_BOX: {
            switch (move) {
                case CTS_LOW:
                    currentState = BOTH_SLOW;
                    break;
                case TX_HIGH:
                    currentState = SLOW_BOX;
                    break;
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
            break;
        }
        case SLOW_BOX: {
            if (move == CTS_LOW) {
                currentState = BOX_PROCESSING;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: SLOW_BOX and move is not CTS_LOW");
            }
            break;
        }
        case BOX_PROCESSING: {
            if (move == CTS_HIGH) {
                currentState = GOOD_TO_TRANSMIT;
            } else {
                // panic
                uart_printf(CONSOLE, "Bad state change: BOX_PROCESSING and move is not CTS_HIGH");
            }
            break;
        }
        default:
            uart_printf(CONSOLE, "State machine is in impossible state\n\r");
            break;
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
    const char sendBuf = toByte(Command::TX);

    sys::Send(marklinServerTid, &sendBuf, 1, nullptr, 0);  // a fake send??
    for (;;) {
        int interruptBits = sys::AwaitEvent(eventChoice);
        ASSERT(interruptBits > 1);
        if (interruptBits >= txCompare &&
            interruptBits <= rtCompare) {  // this comparison could change and remove misoriginal
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
        if (interruptBits == 1) {
            const char sendBuf = toByte(Command::CTS);
            sys::Send(marklinServerTid, &sendBuf, 1, nullptr, 0);
        } else {
            // we get here if we got back from waiting on a CTS event, but the MIS CTS bit was not 1
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

    CharNode marklinClientSlabs[Config::MARKLIN_QUEUE_SIZE];
    Stack<CharNode> freelist;

    for (int i = 0; i < Config::MARKLIN_QUEUE_SIZE; i += 1) {
        freelist.push(&marklinClientSlabs[i]);
    }

    // create notifiers
    int notifierPriority = 32;  // should be higher priority than MarklinServer (I think)
    // otherwise, we may go to receive and then try to reply to our notifier when it wasn't waiting for a reply?
    // update: this might not be the case anymore after the refactor
    uint32_t txNotifier = sys::Create(notifierPriority, MarklinTXNotifier);
    uint32_t rxNotifier = sys::Create(notifierPriority, MarklinRXNotifier);
    uint32_t ctsNotifier = sys::Create(notifierPriority, MarklinCTSNotifier);

    uint32_t getcClient = 0;
    bool txNotifierFlag = true;
    bool rxNotifierFlag = true;
    bool ctsState = true;
    StateMachine ourStateMachine;
    uart_printf(CONSOLE, "Marklin: Done init\n\r");
    for (;;) {
        uint32_t clientTid;
        char msg[2] = {0};
        int msgLen = sys::Receive(&clientTid, msg, 2);

        Command command = commandFromByte(msg[0]);

        if (clientTid == txNotifier || clientTid == ctsNotifier) {
            // Advance state machine on CTS notification
            if (command == Command::CTS) {
                // see
                // https://stackoverflow.com/questions/17024355/is-there-a-logical-boolean-xor-function-in-c-or-c-standard-library
                // ctsState != true;
                // if (ctsState) {
                //     ourStateMachine.changeState(CTS_HIGH);
                // } else {
                //     ourStateMachine.changeState(CTS_LOW);
                // }

                // compiler is throwing a warning ("has no effect") so i'm gonna do it manually
                if (ctsState) {
                    ctsState = false;
                    ourStateMachine.changeState(CTS_LOW);
                } else {
                    ctsState = true;
                    ourStateMachine.changeState(CTS_HIGH);
                }

                // check if we are in a good state to print after that CTS
                // and only print if we have more stuff to send. Otherwise we can just remain in our good state
                if (ourStateMachine.checkState() && !characterQueue.empty()) {
                    // pop from queue and print
                    CharNode* next_char = characterQueue.pop();
                    if (uartTXFull(MARKLIN)) {
                        uart_printf(CONSOLE, "Marklin state machine said we are good to TX, but actually not\n\r");
                    }
                    // rare case this breaks if something else is sending to this register that isn't us
                    // and we interrupt here. Impossible with our design rn.
                    uartPutTX(MARKLIN, next_char->c);
                    freelist.push(next_char);
                    // since we just transmitted, we need to update state machine
                    ourStateMachine.changeState(TX_LOW);
                    // and reply to the notifier to get the next tx interrupt
                    // can the Tx notifier not be waiting on a reply? ideally not after we moved to a good state
                    emptyReply(txNotifier);
                }
                // either way, we reply to the CTS notifier
                // Note: this assumes that CTS will not change while we are in the "good" state.
                // If that assumption breaks then: do not reply when the state is good
                // and only reply after deciding to transmit a char
                emptyReply(ctsNotifier);

            } else if (command == Command::TX) {
                if (ourStateMachine.checkState()) {
                    // we get here upon the first interrupt from initialization
                    // so just make sure not to update the state machine
                    continue;
                }

                // advance state machine
                ourStateMachine.changeState(TX_HIGH);

                // note that we DO NOT reply to the tx notifier until we want another interrupt to happen
                // that is only determined upon transmission of a character, dictated by a CTS interrupt OR a lucky PutC

            } else {
                uart_printf(CONSOLE, "Either marklin CTS or TX notifier did not send a valid command\n\r");
            }

        } else if (clientTid == rxNotifier) {
            if (getcClient != 0) {  // if someone actually wants it
                if (command == Command::RX) {
                    charReply(getcClient, uartGetRX(MARKLIN));  // reply to previous client
                    getcClient = 0;          // so that we don't get here again until we have someone asking for a getc
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
            if (ourStateMachine.checkState()) {
                if (uartTXFull(MARKLIN)) {
                    uart_printf(CONSOLE, "Marklin state machine said we are good to TX, but actually not\n\r");
                }
                uartPutTX(MARKLIN, msg[1]);
                // if state says ok, then great! put it in tx and then update state machine
                ourStateMachine.changeState(TX_LOW);
                // now we want to enable CTS and TX interrupts
                emptyReply(txNotifier);
                // emptyReply(ctsNotifier);
                // what if we reply to the notifier that isn't expecting a reply since CTS would not have sent?
                // on first character in a clean state, TX notifier is waiting on our reply while CTS is NOT.
                // I think we will always reply to the CTS notifier

            } else {
                // We are in the process of transmitting a different char
                // so pop from free list and add to queue
                CharNode* incoming_char = freelist.pop();
                incoming_char->c = msg[1];
                characterQueue.push(incoming_char);
                if (characterQueue.size() == Config::MARKLIN_QUEUE_SIZE) {
                    uart_printf(CONSOLE, "QUEUE LIMIT HIT!!\n\r");
                }
                // // reply to notifier to signal we want interrupts enabled (if it's not notified already)
                // if (!txNotifierFlag) {
                //     // uart_printf(CONSOLE, "Replying to notifier...\r\n");
                //     emptyReply(txNotifier);
                //     txNotifierFlag = true;
                // }
            }
            // reply to client regardless
            charReply(clientTid, static_cast<char>(Reply::SUCCESS));

        } else if (command == Command::GET) {
            // there should only ever be one person asking for getc
            getcClient = clientTid;
            if (!uartRXEmpty(MARKLIN)) {
                charReply(getcClient, uartGetRX(MARKLIN));
            } else {
                // reply to notifier to signal we want interrupts enabled
                if (!rxNotifierFlag) {
                    emptyReply(rxNotifier);
                    rxNotifierFlag = true;
                }
            }
            // DO NOT REPLY TO CLIENT HERE. GETC MUST BE BLOCKING

        } else {
            uart_printf(CONSOLE, "Something broke in the marklin server...\n\r");
        }
    }
}

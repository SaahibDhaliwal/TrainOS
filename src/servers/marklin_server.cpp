#include "marklin_server.h"

#include "config.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "marklin_server_protocol.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "queue.h"
#include "ring_buffer.h"
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

   public:
    StateMachine();
    bool checkState();
    void changeState(interrupt_t move);
};

StateMachine::StateMachine() : currentState(state_t::GOOD_TO_TRANSMIT) {
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
            ASSERT(move == TX_LOW, "Bad state change: GOOD_TO_TRANSMIT and move is not TX_LOW");
            // make sure to transmit this after we check the state and transmit
            currentState = WAITING_ON_BOX;
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
                    ASSERT(move == CTS_LOW || move == TX_HIGH, "Bad state change: WAITING_ON_BOX with incorrect move");
                    break;
            }
            break;
        }
        case BOTH_SLOW: {
            ASSERT(move == TX_HIGH, "Bad state change: BOTH_SLOW and move is not TX_HIGH");
            currentState = BOX_PROCESSING;
            break;
        }
        case SLOW_BOX: {
            ASSERT(move == CTS_LOW, "Bad state change: SLOW_BOX and move is not CTS_LOW");
            currentState = BOX_PROCESSING;
            break;
        }
        case BOX_PROCESSING: {
            ASSERT(move == CTS_HIGH, "Bad state change: BOX_PROCESSING and move is not CTS_HIGH");
            currentState = GOOD_TO_TRANSMIT;
            break;
        }
        default:
            ASSERT(1 == 2, "State machine is in impossible state");
            break;
    }
}

void MarklinRXNotifier() {
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_RX);

    charSend(marklinServerTid, toByte(Command::RX_CONNECT));
    for (;;) {
        sys::AwaitEvent(eventChoice);
        charSend(marklinServerTid, toByte(Command::RX));
    }
}

void MarklinTXNotifier() {
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_TX);

    charSend(marklinServerTid, toByte(Command::TX_CONNECT));

    for (;;) {
        sys::AwaitEvent(eventChoice);
        charSend(marklinServerTid, toByte(Command::TX));
    }
}

void MarklinCTSNotifier() {
    // int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    int marklinServerTid = sys::MyParentTid();
    uint32_t eventChoice = static_cast<uint32_t>(EVENT_ID::MARKLIN_CTS);

    for (;;) {
        sys::AwaitEvent(eventChoice);
        charSend(marklinServerTid, toByte(Command::CTS));
    }
}

void MarklinServer() {
    int registerReturn = name_server::RegisterAs(MARKLIN_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER MARKLIN SERVER\r\n");

    RingBuffer<unsigned char, Config::MARKLIN_QUEUE_SIZE> charQueue;

    // create notifiers
    int notifierPriority = 32;  // should be higher priority than MarklinServer (I think)

    uint32_t txNotifier = sys::Create(notifierPriority, MarklinTXNotifier);
    uint32_t rxNotifier = sys::Create(notifierPriority, MarklinRXNotifier);
    uint32_t ctsNotifier = sys::Create(notifierPriority, MarklinCTSNotifier);

    uint32_t rxClientTid = 0;

    bool ctsState = true;
    bool performingSensorReading = false;
    int sensorCount = 0;
    StateMachine ourStateMachine;

    for (;;) {
        uint32_t clientTid;
        char msg[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, msg, Config::MAX_MESSAGE_LENGTH);

        Command command = commandFromByte(msg[0]);

        switch (command) {
            case Command::TX_CONNECT:
            case Command::RX_CONNECT: {  // initial notifier connection (TX or RX)
                break;
            }
            case Command::CTS: {
                ASSERT(clientTid == ctsNotifier, "GOT CTS COMMAND NOT FROM NOTIFIER");
                if (ctsState) {
                    ctsState = false;
                    ourStateMachine.changeState(CTS_LOW);
                } else {
                    ctsState = true;
                    ourStateMachine.changeState(CTS_HIGH);
                }
                emptyReply(ctsNotifier);
                break;
            }
            case Command::TX: {
                // ASSERT(!charQueue.empty(), "TX WAS ENABLED WHEN WE HAD NO CHARS TO PROCESS");
                ASSERT(!uartTXFull(MARKLIN), "TX SUPPOSED TO BE FREE");
                ASSERT(clientTid == txNotifier, "GOT TX COMMAND NOT FROM NOTIFIER");

                // advance state machine
                ourStateMachine.changeState(TX_HIGH);
                break;
            }
            case Command::RX: {
                ASSERT(rxClientTid != -1, "RX WAS ENABLED WHEN WE HAD NO WAITING CLIENT");
                ASSERT(!uartRXEmpty(MARKLIN), "RX SUPPOSED TO HAVE DATA");
                ASSERT(clientTid == rxNotifier, "GOT RX COMMAND NOT FROM NOTIFIER");

                charReply(rxClientTid, uartGetRX(MARKLIN));  // unblock client
                rxClientTid = 0;
                sensorCount++;
                break;
            }
            case Command::PUT: {
                ASSERT(!charQueue.full(), "QUEUE LIMIT HIT");

                charQueue.push(msg[1]);
                charReply(clientTid, toByte(Reply::SUCCESS));  // unblock client
                break;
            }
            case Command::PUTS: {
                int msgIdx = 1;
                while (msgIdx < msgLen - 1) {
                    ASSERT(!charQueue.full(), "QUEUE LIMIT HIT");
                    charQueue.push(msg[msgIdx]);
                    msgIdx += 1;
                }

                charReply(clientTid, toByte(Reply::SUCCESS));  // unblock client
                break;
            }
            case Command::GET: {
                rxClientTid = clientTid;
                if (!uartRXEmpty(MARKLIN)) {  // already have something to give?
                    charReply(rxClientTid, uartGetRX(MARKLIN));
                    sensorCount++;
                } else {
                    emptyReply(rxNotifier);  // enable RX interrupt
                }
                break;
            }
            case Command::KILL: {
                emptyReply(clientTid);
                sys::Exit();
            }

            default: {
                ASSERT(0, "INVALID COMMAND SENT TO MARKLIN SERVER");
            }
        }

        if (sensorCount == 10) {
            sensorCount = 0;
            performingSensorReading = false;
        }

        // note: we can only do this once since we need to wait for our three interrupts
        // this is for PUSHING to marklin
        if (ourStateMachine.checkState() && !charQueue.empty() && !performingSensorReading) {
            ASSERT(!uartTXFull(MARKLIN), "Marklin state machine said we are good to TX, but actually not");
            unsigned char msgByte = *charQueue.pop();
            if (msgByte == 0x85) {  // if we are about to transmit a sensor request
                performingSensorReading = true;
            }
            uartPutTX(MARKLIN, msgByte);
            ourStateMachine.changeState(TX_LOW);
            emptyReply(txNotifier);  // re-enable TX interrupts
        }
    }
}

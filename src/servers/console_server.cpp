
#include "console_server.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "config.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "queue.h"
#include "ring_buffer.h"
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

void ConsoleBufferDumper() {
    int consoleServerTid = sys::MyParentTid();
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    name_server::RegisterAs("console_dump");

    uint32_t* sender = 0;
    emptyReceive(sender);
    charSend(consoleServerTid, toByte(Command::CONSOLE_DUMP));
    // ASSERT(1 == 2);
    sys::Exit();
}

void ConsoleMeasurementDumper() {
    int consoleServerTid = sys::MyParentTid();
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    name_server::RegisterAs("measure_dump");

    uint32_t* sender = 0;
    emptyReceive(sender);
    charSend(consoleServerTid, toByte(Command::MEASURE_DUMP));
    // ASSERT(1 == 2);
    sys::Exit();
}

void ConsoleServer() {
    int registerReturn = name_server::RegisterAs(CONSOLE_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    RingBuffer<char, Config::CONSOLE_QUEUE_SIZE> charQueue;

    int notifierPriority = 63;  // TODO: think about this prio

    int txNotifier = sys::Create(notifierPriority, ConsoleTXNotifier);
    int rxNotifier = sys::Create(notifierPriority, ConsoleRXNotifier);
    sys::Create(notifierPriority, ConsoleBufferDumper);
    sys::Create(notifierPriority, ConsoleMeasurementDumper);

    int rxClientTid = 0;  // assumption is one command task

    RingBuffer<char, 100000> charQueue2;
    RingBuffer<char, 100000> measurements;

    bool waitForTx = false;

    for (;;) {
        uint32_t clientTid;
        char msg[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, msg, Config::MAX_MESSAGE_LENGTH - 1);

        Command command = commandFromByte(msg[0]);

        switch (command) {
            case Command::TX_CONNECT:
            case Command::RX_CONNECT: {  // initial notifier connection (TX or RX)
                break;
            }
            case Command::TX: {
                // ASSERT(!charQueue.empty(), "TX WAS ENABLED WHEN WE HAD NO CHARS TO PROCESS");
                ASSERT(!uartTXFull(CONSOLE), "TX SUPPOSED TO BE FREE");

                waitForTx = false;
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
                if (!uartRXEmpty(CONSOLE)) {  // already have something to give?
                    charReply(rxClientTid, uartGetRX(CONSOLE));
                } else {
                    emptyReply(rxNotifier);  // enable RX interrupt
                }

                break;
            }
            case Command::CONSOLE_DUMP: {  // only for our buffer dumper?
                while (!charQueue2.empty()) {
                    char ch = *charQueue2.pop();
                    if (ch == '\033') {
                        uart_printf(CONSOLE, "\r\n");
                        uart_printf(CONSOLE, "O: \\033");  // ESC
                    } else if (ch == '\r') {
                        uart_printf(CONSOLE, "Printed: \\r");
                    } else if (ch == '\n') {
                        uart_printf(CONSOLE, "Printed: \\n");
                    } else if (ch == '\t') {
                        uart_printf(CONSOLE, "Printed: \\t");
                    } else if (ch < 32 || ch > 126) {
                        uart_printf(CONSOLE, "Printed: (non-printable: 0x%x)", (unsigned char)ch);
                    } else {
                        uart_printf(CONSOLE, "%c", ch);
                        if (ch == 'H') {
                            uart_printf(CONSOLE, " ");
                        }
                    }
                }
                int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
                ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

                clock_server::Delay(clockServerTid, 10000);
                charSend(clockServerTid, toByte(clock_server::Command::KILL));
                ASSERT(0);

                break;
            }
            case Command::MEASUREMENT: {  // only for our buffer dumper?
                int msgIdx = 1;
                while (msgIdx < msgLen - 1) {
                    measurements.push(msg[msgIdx]);
                    msgIdx += 1;
                }

                charReply(clientTid, toByte(Reply::SUCCESS));  // unblock client
                break;
            }
            case Command::MEASURE_DUMP: {  // only for our buffer dumper?
                while (!measurements.empty()) {
                    char ch = *measurements.pop();
                    uart_printf(CONSOLE, "%c", ch);
                }

                ASSERT(0);

                break;
            }
            case Command::KILL: {
                emptyReply(clientTid);
                sys::Exit();
                break;  // for compiler warning
            }
            default: {
                ASSERT(0, "INVALID COMMAND SENT TO CONSOLE SERVER");
            }
        }

        bool drainedAny = false;
        while (!charQueue.empty() && !uartTXFull(CONSOLE)) {  // drain as much as possible
            unsigned char ch = *charQueue.pop();
            uartPutTX(CONSOLE, ch);
            charQueue2.push(ch);  // onto our logger
            drainedAny = true;
        }

        // TODO: FIX ME
        if (!charQueue.empty() && !waitForTx && command != Command::TX_CONNECT && drainedAny) {
            emptyReply(txNotifier);  // re-enable TX interrupts
            waitForTx = true;
            drainedAny = false;
        }
    }
}

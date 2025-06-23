#include "command_server.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "command_server_protocol.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor_server.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "train.h"
#include "turnout.h"

using namespace command_server;

bool processInputCommand(char* command, Train* trains, Turnout* turnouts, int marklinServerTid,
                         int printerProprietorTid, int clockServerTid, RingBuffer<int, MAX_TRAINS>* reversingTrains) {
    if (strncmp(command, "tr ", 3) == 0) {
        const char* cur = &command[3];

        if (*cur < '0' || *cur > '9') return false;

        int trainNumber = 0;
        while (*cur >= '0' && *cur <= '9') {
            if (trainNumber > 255) return false;
            trainNumber = trainNumber * 10 + (*cur - '0');
            cur++;
        }

        int trainIdx = trainNumToIndex(trainNumber);
        if (trainIdx == -1) return false;

        if (*cur != ' ') return false;
        cur++;

        if (*cur < '0' || *cur > '9') return false;  // speed
        int trainSpeed = 0;

        while (*cur >= '0' && *cur <= '9') {
            trainSpeed = trainSpeed * 10 + (*cur - '0');
            cur++;
        }

        if (trainSpeed > TRAIN_SPEED_14) return false;

        if (*cur != '\0') return false;

        marklin_server::setTrainSpeed(marklinServerTid, (char)trainSpeed + 16, (char)trainNumber);

    } else if (strncmp(command, "rv ", 3) == 0) {
        const char* cur = &command[3];

        if (*cur < '0' || *cur > '9') return false;

        int trainNumber = 0;
        while (*cur >= '0' && *cur <= '9') {
            if (trainNumber > 255) return false;
            trainNumber = trainNumber * 10 + (*cur - '0');
            cur++;
        }

        int trainIdx = trainNumToIndex(trainNumber);
        if (trainIdx == -1) return false;

        if (*cur != '\0') return false;

        marklin_server::setTrainSpeed(marklinServerTid, (char)16, (char)trainNumber);
        reversingTrains->push(trainIdx);
        sys::Create(40, &marklin_server::reverseTrainTask);

    } else if (strncmp(command, "sw ", 3) == 0) {
        const char* cur = &command[3];

        if (*cur < '0' || *cur > '9') return false;

        int turnoutNumber = 0;
        while (*cur >= '0' && *cur <= '9') {
            if (turnoutNumber > 255) return false;
            turnoutNumber = turnoutNumber * 10 + (*cur - '0');
            cur++;
        }

        int turnoutIndex = turnoutIdx(turnoutNumber);
        if (turnoutIndex == -1) return false;

        if (*cur != ' ') return false;
        cur++;

        if (*cur != 'S' && *cur != 's' && *cur != 'C' && *cur != 'c') return false;  // direction

        Command_Byte turnoutDirection = SWITCH_STRAIGHT;
        if (*cur == 'c' || *cur == 'C') turnoutDirection = SWITCH_CURVED;

        cur++;

        if (*cur != '\0') return false;

        marklin_server::setTurnout(marklinServerTid, turnoutDirection, (char)turnoutNumber);
        clock_server::Delay(clockServerTid, 20);

        if (turnoutDirection == SWITCH_STRAIGHT) {
            turnouts[turnoutIndex].state = STRAIGHT;
        } else {
            turnouts[turnoutIndex].state = CURVED;
        }

        WITH_HIDDEN_CURSOR(printerProprietorTid, update_turnout(turnouts, turnoutIndex, printerProprietorTid));

        switch (turnoutNumber) {
            case 153: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, (char)154);
                    turnouts[turnoutIndex + 1].state = STRAIGHT;

                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, (char)154);
                    turnouts[turnoutIndex + 1].state = CURVED;
                }
                clock_server::Delay(clockServerTid, 20);
                WITH_HIDDEN_CURSOR(printerProprietorTid,
                                   update_turnout(turnouts, turnoutIndex + 1, printerProprietorTid));

                break;
            }
            case 154: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, (char)153);
                    turnouts[turnoutIndex - 1].state = STRAIGHT;

                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, (char)153);
                    turnouts[turnoutIndex - 1].state = CURVED;
                }
                clock_server::Delay(clockServerTid, 20);
                WITH_HIDDEN_CURSOR(printerProprietorTid,
                                   update_turnout(turnouts, turnoutIndex - 1, printerProprietorTid));

                break;
            }
            case 155: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, (char)156);
                    turnouts[turnoutIndex + 1].state = STRAIGHT;

                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, (char)156);
                    turnouts[turnoutIndex + 1].state = CURVED;
                }
                clock_server::Delay(clockServerTid, 20);
                WITH_HIDDEN_CURSOR(printerProprietorTid,
                                   update_turnout(turnouts, turnoutIndex + 1, printerProprietorTid));
                break;
            }
            case 156: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, (char)155);
                    turnouts[turnoutIndex - 1].state = STRAIGHT;

                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, (char)155);
                    turnouts[turnoutIndex - 1].state = CURVED;
                }
                clock_server::Delay(clockServerTid, 20);
                WITH_HIDDEN_CURSOR(printerProprietorTid,
                                   update_turnout(turnouts, turnoutIndex - 1, printerProprietorTid));

                break;
            }
            default: {
                break;
            }
        }

        marklin_server::solenoidOff(marklinServerTid);
    } else {
        return false;
    }

    return true;
}

void CommandTask() {
    uint32_t printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    uint32_t consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    ASSERT(consoleTid >= 0, "UNABLE TO GET CONSOLE_SERVER_NAME\r\n");

    uint32_t commandServerTid = name_server::WhoIs(COMMAND_SERVER_NAME);
    ASSERT(commandServerTid >= 0, "UNABLE TO GET COMMAND_SERVER_NAME\r\n");

    print_clear_command_prompt(printerProprietorTid);

    int sensorServerTid = sys::Create(20, &SensorServer);

    char userInput[Config::MAX_COMMAND_LENGTH];
    int userInputIdx = 0;
    for (;;) {
        char ch = console_server::Getc(consoleTid, 0);

        if (ch >= 0) {
            // if (ch == 'q' || ch == 'Q') {
            //     return 0;
            // }

            // if (isInitializing) continue;

            if (ch == '\r') {
                print_clear_command_prompt(printerProprietorTid);
                userInput[userInputIdx] = '\0';  // mark end of user input command

                if (userInput[0] == 'd') {
                    int tid = name_server::WhoIs("console_dump");
                    emptySend(tid);
                }

                char replyChar;
                sys::Send(commandServerTid, userInput, userInputIdx, &replyChar, 1);

                WITH_HIDDEN_CURSOR(
                    printerProprietorTid,
                    print_command_feedback(printerProprietorTid, command_server::replyFromByte(replyChar)));

                userInputIdx = 0;
            } else if (ch == '\b' || ch == 0x7F) {
                if (userInputIdx > 0) {
                    backspace(printerProprietorTid);
                    userInputIdx -= 1;
                }
            } else if (ch >= 0x20 && ch <= 0x7E && userInputIdx < 254) {
                userInput[userInputIdx++] = ch;

                printer_proprietor::printC(printerProprietorTid, 0, ch);
            }  // if
        }  // if
    }
}

void CommandServer() {
    int registerReturn = name_server::RegisterAs(COMMAND_SERVER_NAME);

    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts
    initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);

    Train trains[MAX_TRAINS];  // trains
    initializeTrains(trains, marklinServerTid);

    RingBuffer<int, MAX_TRAINS> reversingTrains;  // trains

    int terminalTid = sys::Create(20, &CommandTask);
    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';

        if (clientTid == terminalTid) {
            bool validCommand = processInputCommand(receiveBuffer, trains, turnouts, marklinServerTid,
                                                    printerProprietorTid, clockServerTid, &reversingTrains);

            if (!validCommand) {
                charReply(clientTid, toByte(Reply::FAILURE));
            } else {
                charReply(clientTid, toByte(Reply::SUCCESS));
            }
        } else {
            ASSERT(!reversingTrains.empty(), "HAD A REVERSING PROCESS WITH NO REVERSING TRAINS\r\n");
            int reversingTrainIdx = *reversingTrains.pop();
            int trainSpeed = trains[reversingTrainIdx].speed;
            int trainNumber = trains[reversingTrainIdx].id;
            marklin_server::setTrainReverseAndSpeed(marklinServerTid, (char)trainSpeed, (char)trainNumber);
        }
    }

    sys::Exit();
}
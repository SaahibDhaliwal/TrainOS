#include "command_server.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "command_client.h"
#include "command_server_protocol.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "localization_server.h"
#include "localization_server_protocol.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "train.h"
#include "turnout.h"
using namespace command_server;

bool processInputCommand(char* command, int marklinServerTid, int printerProprietorTid, int clockServerTid,
                         int localizationTid) {
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

        if (trainSpeed > 14) return false;

        if (*cur != '\0') return false;

        localization_server::setTrainSpeed(localizationTid, trainSpeed + 16, trainNumber);

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

        localization_server::reverseTrain(localizationTid, trainNumber);

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

        marklin_server::setTurnout(marklinServerTid, turnoutDirection, turnoutNumber);
        clock_server::Delay(clockServerTid, 20);
        printer_proprietor::updateTurnout(printerProprietorTid, turnoutDirection, turnoutIndex);

        switch (turnoutNumber) {
            case 153: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, 154);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex + 1);
                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, 154);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex + 1);
                }
                clock_server::Delay(clockServerTid, 20);

                break;
            }
            case 154: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, 153);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex - 1);
                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, 153);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex - 1);
                }
                clock_server::Delay(clockServerTid, 20);

                break;
            }
            case 155: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, 156);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex + 1);
                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, 156);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex + 1);
                }
                clock_server::Delay(clockServerTid, 20);
                break;
            }
            case 156: {
                if (turnoutDirection == SWITCH_CURVED) {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, 155);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex - 1);
                } else {
                    marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, 155);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex - 1);
                }
                clock_server::Delay(clockServerTid, 20);
                break;
            }
            default: {
                break;
            }
        }

        marklin_server::solenoidOff(marklinServerTid);
    } else if (strncmp(command, "q", 1) == 0) {
        charSend(clockServerTid, clock_server::toByte(clock_server::Command::KILL));
        charSend(printerProprietorTid, toByte(printer_proprietor::Command::KILL));
        charSend(marklinServerTid, marklin_server::toByte(marklin_server::Command::KILL));
        charSend(Config::NAME_SERVER_TID, name_server::toByte(name_server::Command::KILL));
        sys::Quit();
        ASSERT(0, "WE SHOULD NEVER GET HERE IF QUIT WORKS\r\n");

    } else {
        return false;
    }

    return true;
}

void CommandServer() {
    int registerReturn = name_server::RegisterAs(COMMAND_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    uint32_t terminalTid = sys::Create(20, &CommandTask);
    uint32_t localizationTid = sys::Create(25, &LocalizationServer);

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';

        if (clientTid == terminalTid) {
            bool validCommand = processInputCommand(receiveBuffer, marklinServerTid, printerProprietorTid,
                                                    clockServerTid, localizationTid);

            if (!validCommand) {
                charReply(clientTid, toByte(Reply::FAILURE));
            } else {
                charReply(clientTid, toByte(Reply::SUCCESS));
            }
        }
    }

    sys::Exit();
}
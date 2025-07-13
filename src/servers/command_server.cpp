#include "command_server.h"

#include "clock_server_protocol.h"
#include "command.h"
#include "command_client.h"
#include "command_server_protocol.h"
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
        if (trainSpeed != 14 && trainSpeed != 8 && trainSpeed != 0) return false;

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

        localization_server::setTurnout(localizationTid, turnoutDirection, turnoutNumber);
        clock_server::Delay(clockServerTid, 20);  // why is this here?
        printer_proprietor::updateTurnout(printerProprietorTid, turnoutDirection, turnoutIndex);

        switch (turnoutNumber) {
            case 153: {
                if (turnoutDirection == SWITCH_CURVED) {
                    localization_server::setTurnout(localizationTid, SWITCH_STRAIGHT, 154);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex + 1);
                } else {
                    localization_server::setTurnout(localizationTid, SWITCH_CURVED, 154);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex + 1);
                }
                clock_server::Delay(clockServerTid, 20);

                break;
            }
            case 154: {
                if (turnoutDirection == SWITCH_CURVED) {
                    localization_server::setTurnout(localizationTid, SWITCH_STRAIGHT, 153);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex - 1);
                } else {
                    localization_server::setTurnout(localizationTid, SWITCH_CURVED, 153);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex - 1);
                }
                clock_server::Delay(clockServerTid, 20);

                break;
            }
            case 155: {
                if (turnoutDirection == SWITCH_CURVED) {
                    localization_server::setTurnout(localizationTid, SWITCH_STRAIGHT, 156);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex + 1);
                } else {
                    localization_server::setTurnout(localizationTid, SWITCH_CURVED, 156);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex + 1);
                }
                clock_server::Delay(clockServerTid, 20);
                break;
            }
            case 156: {
                if (turnoutDirection == SWITCH_CURVED) {
                    localization_server::setTurnout(localizationTid, SWITCH_STRAIGHT, 155);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_STRAIGHT, turnoutIndex - 1);
                } else {
                    localization_server::setTurnout(localizationTid, SWITCH_CURVED, 155);
                    printer_proprietor::updateTurnout(printerProprietorTid, SWITCH_CURVED, turnoutIndex - 1);
                }
                clock_server::Delay(clockServerTid, 20);
                break;
            }
            default: {
                break;
            }
        }

        localization_server::solenoidOff(localizationTid);
    } else if (strncmp(command, "q", 1) == 0) {
        charSend(clockServerTid, clock_server::toByte(clock_server::Command::KILL));
        charSend(printerProprietorTid, toByte(printer_proprietor::Command::KILL));
        charSend(marklinServerTid, marklin_server::toByte(marklin_server::Command::KILL));
        charSend(Config::NAME_SERVER_TID, name_server::toByte(name_server::Command::KILL));
        sys::Quit();
        ASSERT(0, "WE SHOULD NEVER GET HERE IF QUIT WORKS\r\n");

    } else if (strncmp(command, "reset", 5) == 0) {
        localization_server::resetTrack(localizationTid);

    } else if (strncmp(command, "stop ", 5) == 0) {
        const char* cur = &command[5];

        if (*cur < '0' || *cur > '9') return false;

        int trainNumber = 0;
        while (*cur >= '0' && *cur <= '9') {
            if (trainNumber > 255) return false;
            trainNumber = trainNumber * 10 + (*cur - '0');
            cur++;
        }

        if (*cur != ' ') return false;
        cur++;

        char box = *cur;
        if ((box > 'M')) box = box - 32;  // forces uppercase
        if (box < 'A' || ((box > 'E') && (box < 'M'))) return false;
        cur++;

        if (box == 'B' && (*cur == 'R' || *cur == 'r')) {
            box = 'F';  // bad practice but oh well
            cur++;
        }

        if (box == 'M' && (*cur == 'R' || *cur == 'r')) {
            box = 'G';
            cur++;
        }

        if (box == 'E') {
            if (*cur == 'N' || *cur == 'n') {
                box = 'H';
                cur++;
                return false;
            } else if (*cur == 'X' || *cur == 'x') {
                box = 'I';
                cur++;
            }
        }

        // ASSERT(0, "box: %c nextchar: %c", box, *cur);

        if (*cur < '0' || *cur > '9') return false;
        int sensorNum = 0;
        while (*cur >= '0' && *cur <= '9') {
            if (sensorNum > 16 && box >= 'A' && box <= 'E') return false;
            if (sensorNum > 156 && (box == 'F' || box == 'G')) return false;
            sensorNum = sensorNum * 10 + (*cur - '0');
            cur++;
        }
        // ASSERT(0, "sensornum: %d", sensorNum);
        if (sensorNum == 0) return false;
        if ((box == 'F' || box == 'G') && (sensorNum < 153 && sensorNum > 18)) return false;

        if ((box == 'B' && sensorNum == 7) || (box == 'B' && sensorNum == 11) || (box == 'B' && sensorNum == 9) ||
            (box == 'A' && sensorNum == 13) || (box == 'A' && sensorNum == 1) || (box == 'C' && sensorNum == 4))
            return false;

#ifndef TRACKA
        // checks if an incorrect exit node is passed on track B
        if ((box == 'H' || box == 'I') && (sensorNum == 6 || sensorNum == 8)) return false;
        if ((box == 'A' && sensorNum == 16) || (box == 'A' && sensorNum == 11)) return false;

#endif
        if ((box == 'I') && (sensorNum > 10)) return false;

        if (*cur != ' ') return false;
        cur++;

        int offset = 0;
        bool negativeFlag = false;
        if (*cur == '-') {
            negativeFlag = true;
            cur++;
            if (*cur == '\0') return false;
        }
        if (*cur < '0' || *cur > '9') return false;
        while (*cur >= '0' && *cur <= '9') {
            if (offset > 1000000000000) return false;
            offset = offset * 10 + (*cur - '0');
            cur++;
        }
        if (negativeFlag) offset *= -1;
        // ASSERT(0, "trainnum: %u box: %c, sensorNum: %u, offset: %d", trainNumber, box, sensorNum, offset);
        localization_server::setStopLocation(localizationTid, trainNumber, box, sensorNum, offset);

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
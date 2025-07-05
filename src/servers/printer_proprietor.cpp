#include "printer_proprietor.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "config.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "display_refresher.h"
#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_helpers.h"
#include "printer_proprietor_protocol.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "turnout.h"

using namespace printer_proprietor;

void PrinterProprietor() {
    int registerReturn = name_server::RegisterAs(PRINTER_PROPRIETOR_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER PRINTER PROPRIETOR SERVER\r\n");

    int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    ASSERT(consoleServerTid >= 0, "UNABLE TO GET CONSOLE_SERVER_NAME\r\n");

    sys::Create(20, &displayRefresher);

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts

    Sensor sensorBuffer[SENSOR_BUFFER_SIZE];
    initialize_sensors(sensorBuffer);
    int sensorBufferIdx = 0;
    bool isSensorBufferParityEven = true;

    unsigned int measurementMessages = 0;

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';

        Command command = commandFromByte(receiveBuffer[0]);

        switch (command) {
            case Command::PRINTS: {
                const char* msg = &receiveBuffer[1];
                console_server::Puts(consoleServerTid, CONSOLE, msg);

                break;
            }
            case Command::PRINTC: {
                console_server::Putc(consoleServerTid, CONSOLE, receiveBuffer[1]);

                break;
            }
            case Command::KILL: {
                charSend(consoleServerTid, console_server::toByte(console_server::Command::KILL));
                emptyReply(clientTid);
                sys::Exit();
                break;
            }
            case Command::COMMAND_FEEDBACK: {
                WITH_HIDDEN_CURSOR(consoleServerTid,
                                   command_feedback(consoleServerTid, command_server::replyFromByte(receiveBuffer[1])));
                break;
            }
            case Command::CLEAR_COMMAND_PROMPT: {
                print_clear_command_prompt(consoleServerTid);
                break;
            }
            case Command::UPDATE_TURNOUT: {
                char turnoutCommand = receiveBuffer[1];

                unsigned int turnoutIdx = 0;
                a2ui(receiveBuffer + 2, 10, &turnoutIdx);

                if (turnoutCommand == SWITCH_CURVED) {
                    turnouts[turnoutIdx].state = SwitchState::CURVED;
                } else {
                    turnouts[turnoutIdx].state = SwitchState::STRAIGHT;
                }

                WITH_HIDDEN_CURSOR(consoleServerTid, update_turnout(turnouts, turnoutIdx, consoleServerTid));
                break;
            }
            case Command::UPDATE_SENSOR: {
                // char box = receiveBuffer[1];

                // unsigned int sensorNum = 0;
                // a2ui(receiveBuffer + 2, 10, &sensorNum);

                // int prevSensorIdx = (sensorBufferIdx - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;

                // if (sensorBuffer[prevSensorIdx].box != box ||
                //     sensorBuffer[prevSensorIdx].num != sensorNum) {  // has the sensor changed?
                //     if (sensorBufferIdx == 0) {                      // starting from the top, we can switch colour
                //         isSensorBufferParityEven = !isSensorBufferParityEven;
                //     }
                //     sensorBuffer[sensorBufferIdx].box = box;
                //     sensorBuffer[sensorBufferIdx].num = sensorNum;
                //     WITH_HIDDEN_CURSOR(consoleServerTid, update_sensor(sensorBuffer, sensorBufferIdx,
                //     consoleServerTid,
                //                                                        isSensorBufferParityEven));
                //     sensorBufferIdx = (sensorBufferIdx + 1) % SENSOR_BUFFER_SIZE;
                // }

                if (sensorBufferIdx == 0) {  // starting from the top, we can switch colour
                    isSensorBufferParityEven = !isSensorBufferParityEven;
                }

                char* firstMsgEnd = receiveBuffer;
                while (*firstMsgEnd != '#') {
                    firstMsgEnd++;
                }
                *firstMsgEnd = '\0';

                WITH_HIDDEN_CURSOR(consoleServerTid, update_sensor(consoleServerTid, &receiveBuffer[1], sensorBufferIdx,
                                                                   isSensorBufferParityEven));
                WITH_HIDDEN_CURSOR(consoleServerTid,
                                   update_sensor_time(consoleServerTid, &firstMsgEnd[1], sensorBufferIdx));

                sensorBufferIdx = (sensorBufferIdx + 1) % SENSOR_BUFFER_SIZE;

                break;
            }
            case Command::STARTUP_PRINT: {
                startup_print(consoleServerTid);
                break;
            }
            case Command::REFRESH_CLOCKS: {
                unsigned int idleTime = 0;
                a2ui(receiveBuffer + 1, 10, &idleTime);
                WITH_HIDDEN_CURSOR(consoleServerTid, refresh_clocks(consoleServerTid, idleTime));
                break;
            }
            case Command::BACKSPACE: {
                back_space(consoleServerTid);
                break;
            }
            case Command::MEASUREMENT: {
                print_measurement(consoleServerTid, measurementMessages, &receiveBuffer[1]);
                measurementMessages += 1;

                break;
            }
            case Command::UPDATE_TRAIN: {
                WITH_HIDDEN_CURSOR(consoleServerTid, print_train_status(consoleServerTid, &receiveBuffer[1]));
                break;
            }
            default: {
                ASSERT(0, "[Printer Proprietor]: Unknown Command!\r\n");
                break;
            }
        }
        emptyReply(clientTid);
    }

    sys::Exit();
}
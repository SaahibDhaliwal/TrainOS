#include "printer_proprietor.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "config.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "cursor.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "name_server_protocol.h"
#include "sensor_server.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "turnout.h"
#include "uptime.h"

using namespace printer_proprietor;

#define COMMAND_PROMPT_START_ROW 29
#define COMMAND_PROMPT_START_COL 0

void displayRefresher() {
    uint32_t clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    uint32_t printerTid = sys::MyParentTid();

    for (;;) {
        clock_server::Delay(clockServerTid, 10);
        refreshClocks(printerTid);
    }
}

void refresh_clocks(int tid, unsigned int idleTime) {
    update_idle_percentage(idleTime, tid);
    update_uptime(tid, timerGet());
}

void command_feedback(int tid, command_server::Reply reply) {
    cursor_down_one(tid);
    clear_current_line(tid);
    if (reply == command_server::Reply::SUCCESS) {
        cursor_soft_green(tid);
        console_server::Puts(tid, 0, "✔ Command accepted");
    } else {
        cursor_soft_red(tid);
        console_server::Puts(tid, 0, "✖ Invalid command");
    }  // if
}

void startup_print(int consoleTid) {
    hide_cursor(consoleTid);
    clear_screen(consoleTid);
    cursor_top_left(consoleTid);

    print_ascii_art(consoleTid);

    cursor_white(consoleTid);
    // uartPutS(consoleTid, "Press 'q' to reboot\n");
    print_uptime(consoleTid);
    print_idle_percentage(consoleTid);

    print_turnout_table(consoleTid);
    print_sensor_table(consoleTid);

    cursor_soft_pink(consoleTid);
    print_command_prompt_blocked(consoleTid);
    cursor_white(consoleTid);
}

void PrinterProprietor() {
    int registerReturn = name_server::RegisterAs(PRINTER_PROPRIETOR_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER PRINTER PROPRIETOR SERVER\r\n");

    int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    ASSERT(consoleServerTid >= 0, "UNABLE TO GET CONSOLE_SERVER_NAME\r\n");

    uint32_t displayRefresherTid = sys::Create(20, &displayRefresher);

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts

    Sensor sensorBuffer[SENSOR_BUFFER_SIZE];
    initialize_sensors(sensorBuffer);
    int sensorBufferIdx = 0;
    bool isSensorBufferParityEven = true;

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
                    turnouts[turnoutIdx].state = CURVED;
                } else {
                    turnouts[turnoutIdx].state = STRAIGHT;
                }

                WITH_HIDDEN_CURSOR(consoleServerTid, update_turnout(turnouts, turnoutIdx, consoleServerTid));
                break;
            }
            case Command::UPDATE_SENSOR: {
                char box = receiveBuffer[1];

                unsigned int sensorNum = 0;
                a2ui(receiveBuffer + 2, 10, &sensorNum);

                int prevSensorIdx = (sensorBufferIdx - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;

                if (sensorBuffer[prevSensorIdx].box != box ||
                    sensorBuffer[prevSensorIdx].num != sensorNum) {  // has the sensor changed?
                    if (sensorBufferIdx == 0) {                      // starting from the top, we can switch colour
                        isSensorBufferParityEven = !isSensorBufferParityEven;
                    }
                    sensorBuffer[sensorBufferIdx].box = box;
                    sensorBuffer[sensorBufferIdx].num = sensorNum;
                    WITH_HIDDEN_CURSOR(consoleServerTid, update_sensor(sensorBuffer, sensorBufferIdx, consoleServerTid,
                                                                       isSensorBufferParityEven));
                    sensorBufferIdx = (sensorBufferIdx + 1) % SENSOR_BUFFER_SIZE;
                }

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
            default: {
                ASSERT(0, "[Printer Proprietor]: Unknown Command!\r\n");
                break;
            }
        }
        emptyReply(clientTid);
    }

    sys::Exit();
}
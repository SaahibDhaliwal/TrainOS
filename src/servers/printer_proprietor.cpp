#include "printer_proprietor.h"

#include "command.h"
#include "config.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "cursor.h"
#include "idle_time.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "sensor.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "turnout.h"
#include "uptime.h"

using namespace printer_proprietor;

void startup_print(int consoleTid, Turnout* turnouts) {
    hide_cursor(consoleTid);
    clear_screen(consoleTid);
    cursor_top_left(consoleTid);

    print_ascii_art(consoleTid);

    cursor_white(consoleTid);
    // uartPutS(consoleTid, "Press 'q' to reboot\n");
    print_uptime(consoleTid);
    print_idle_percentage(consoleTid);

    print_turnout_table(turnouts, consoleTid);
    print_sensor_table(consoleTid);

    cursor_soft_pink(consoleTid);
    print_command_prompt_blocked(consoleTid);
    cursor_white(consoleTid);
}

void PrinterProprietor() {
    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts
    // initialize_turnouts(turnouts, &marklinQueue);
    int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);

    startup_print(consoleServerTid, turnouts);  // initial display

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
            default: {
                ASSERT(0, "[Name Server]: Unknown Command!\r\n");
                break;
            }
        }
    }

    sys::Exit();
}
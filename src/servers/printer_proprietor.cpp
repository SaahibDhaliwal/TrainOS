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
#include "printer_proprietor_protocol.h"
#include "sensor.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "turnout.h"
#include "uptime.h"

using namespace printer_proprietor;

void displayRefresher() {
    uint32_t clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    uint32_t printerTid = sys::MyParentTid();

    for (;;) {
        clock_server::Delay(clockServerTid, 10);
        update_idle_percentage(sys::GetIdleTime(), printerTid);
        update_uptime(printerTid, timerGet());
    }
}

void PrinterProprietor() {
    int registerReturn = name_server::RegisterAs(PRINTER_PROPRIETOR_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER PRINTER PROPRIETOR SERVER\r\n");

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];  // turnouts
    // initialize_turnouts(turnouts, &marklinQueue);
    int consoleServerTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    ASSERT(consoleServerTid >= 0, "UNABLE TO GET CONSOLE_SERVER_NAME\r\n");

    uint32_t displayRefresherTid = sys::Create(20, &displayRefresher);

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

            default: {
                ASSERT(0, "[Printer Proprietor]: Unknown Command!\r\n");
                break;
            }
        }
        emptyReply(clientTid);
    }

    sys::Exit();
}
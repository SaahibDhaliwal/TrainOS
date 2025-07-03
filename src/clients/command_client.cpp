#include "command_client.h"

#include "command.h"
#include "command_server.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "sys_call_stubs.h"
#include "test_utils.h"

void CommandTask() {
    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int consoleTid = name_server::WhoIs(CONSOLE_SERVER_NAME);
    ASSERT(consoleTid >= 0, "UNABLE TO GET CONSOLE_SERVER_NAME\r\n");

    int commandServerTid = name_server::WhoIs(COMMAND_SERVER_NAME);
    ASSERT(commandServerTid >= 0, "UNABLE TO GET COMMAND_SERVER_NAME\r\n");

    printer_proprietor::clearCommandPrompt(printerProprietorTid);

    char userInput[Config::MAX_COMMAND_LENGTH];
    int userInputIdx = 0;
    for (;;) {
        char ch = console_server::Getc(consoleTid, 0);

        if (ch >= 0) {
            if (ch == '\r') {
                printer_proprietor::clearCommandPrompt(printerProprietorTid);

                userInput[userInputIdx] = '\0';  // mark end of user input command

                if (userInput[0] == 'd') {
                    int tid = name_server::WhoIs("console_dump");
                    emptySend(tid);
                } else if (userInput[0] == 'm') {
                    int tid = name_server::WhoIs("measure_dump");
                    emptySend(tid);
                } else {
                    char replyChar;
                    sys::Send(commandServerTid, userInput, userInputIdx, &replyChar, 1);
                    printer_proprietor::commandFeedback(command_server::replyFromByte(replyChar), printerProprietorTid);
                }

                userInputIdx = 0;
            } else if (ch == '\b' || ch == 0x7F) {
                if (userInputIdx > 0) {
                    printer_proprietor::backspace(printerProprietorTid);
                    userInputIdx -= 1;
                }
            } else if (ch >= 0x20 && ch <= 0x7E && userInputIdx < 254) {
                userInput[userInputIdx++] = ch;

                printer_proprietor::printC(printerProprietorTid, 0, ch);
            }  // if
        }  // if
    }
}
#include "printer_proprietor_protocol.h"

#include "config.h"
#include "sys_call_stubs.h"

namespace printer_proprietor {

char toByte(Command c) {
    return static_cast<char>(c);
}

char toByte(Reply r) {
    return static_cast<char>(r);
}

Command commandFromByte(char c) {
    return (c < static_cast<char>(Command::COUNT)) ? static_cast<Command>(c) : Command::UNKNOWN_COMMAND;
}

Reply replyFromByte(char c) {
    return (c < static_cast<char>(Reply::COUNT)) ? static_cast<Reply>(c) : Reply::UNKNOWN_REPLY;
}

void printS(const char* str) {
}

int printS(int tid, int channel, const char* str) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::PRINTS);
    strcpy(sendBuf + 1, str);

    char reply = 0;
    int response = sys::Send(tid, sendBuf, strlen(str) + 2, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return response;
    } else {
        return 0;
    }
}

}  // namespace printer_proprietor
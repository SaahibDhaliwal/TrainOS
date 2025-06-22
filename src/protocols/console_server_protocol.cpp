#include "console_server_protocol.h"

#include "config.h"
#include "sys_call_stubs.h"
#include "util.h"

namespace console_server {

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

int Getc(int tid, int channel) {
    const char sendBuf = toByte(Command::GET);
    char reply = 0;

    int response = sys::Send(tid, &sendBuf, 1, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return -1;
    }

    return reply;
}

int Putc(int tid, int channel, unsigned char ch) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::PUT);
    sendBuf[1] = ch;
    if (ch == '\0') return 0;  // don't send a char if it's just the end of a string

    char reply = 0;
    int response = sys::Send(tid, sendBuf, 2, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return response;
    } else {
        return 0;
    }
}

int Puts(int tid, int channel, const char* str) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::PUTS);
    strcpy(sendBuf + 1, str);

    char reply = 0;
    int response = sys::Send(tid, sendBuf, strlen(str) + 2, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return response;
    } else {
        return 0;
    }
}

}  // namespace console_server
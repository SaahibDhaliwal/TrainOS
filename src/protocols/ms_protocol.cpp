#include "protocols/ms_protocol.h"

#include "config.h"
#include "sys_call_stubs.h"
#include "util.h"

namespace marklin_server {

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

    char reply = 0;
    int response = sys::Send(tid, sendBuf, 2, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return response;
    } else {
        return 0;
    }
}

}  // namespace marklin_server
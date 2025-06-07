#include "protocols/ns_protocol.h"

#include "config.h"
#include "sys_call_stubs.h"

namespace name_server {

char toByte(Command c) {
    return static_cast<char>(c);
}

Command commandFromByte(char c) {
    return (c < static_cast<char>(Command::COUNT)) ? static_cast<Command>(c) : Command::UNKNOWN_COMMAND;
}

int RegisterAs(const char *name) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::REGISTER);
    strcpy(sendBuf + 1, name);

    int response = sys::Send(Config::NAME_SERVER_TID, sendBuf, strlen(name) + 2, nullptr, 0);

    if (response < 0) {
        return -1;
    }
    return 0;
}

int WhoIs(const char *name) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::WHO_IS);
    strcpy(sendBuf + 1, name);

    char replyBuf[4] = {0};
    int response = sys::Send(Config::NAME_SERVER_TID, sendBuf, strlen(name) + 2, replyBuf, 3);

    if (response < 0) {
        return -1;
    }

    unsigned int tid;
    a2ui(replyBuf, 10, &tid);
    return tid;
}
}
#include "protocols/cs_protocol.h"

#include "config.h"
#include "sys_call_stubs.h"
#include "util.h"

namespace clock_server {

char toByte(Command c) {
    return static_cast<char>(c);
}

char toByte(Reply r) {
    return static_cast<char>(r);
}

Command commandFromByte(char c) {
    return (c < static_cast<char>(Command::COUNT)) ? static_cast<Command>(c) : Command::UNKNOWN_COMMAND;
}

// Might not need this right now, since clock server should always succeed
Reply replyFromByte(char c) {
    return (c < static_cast<char>(Reply::COUNT)) ? static_cast<Reply>(c) : Reply::UNKNOWN_REPLY;
}

int Time(int tid) {
    const char sendBuf = toByte(Command::TIME);
    char replyBuf[32] = {0};

    int response = sys::Send(tid, &sendBuf, 1, replyBuf, 32);

    if (response < 0) {  // if Send() cannot reach the TID
        return -1;
    }

    unsigned int time = 0;
    a2ui(replyBuf, 10, &time);
    return time;
}

int Delay(int tid, int ticks) {
    if (ticks < 0) {  // negative delay
        return -2;
    }
    char sendBuf[33] = {0};
    sendBuf[0] = toByte(Command::DELAY);
    char tick_string[32] = {0};
    ui2a(ticks, 10, tick_string);
    strcpy(sendBuf + 1, tick_string);

    char replyBuf[32] = {0};
    int response = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, replyBuf, 32);

    if (response < 0) {  // if Send() cannot reach the TID
        return -1;
    }

    unsigned int time = 0;
    a2ui(replyBuf, 10, &time);
    return time;
}

int DelayUntil(int tid, int ticks) {
    if (ticks < 0) {  // negative delay
        return -2;
    }
    char sendBuf[33] = {0};
    sendBuf[0] = toByte(Command::DELAY_UNTIL);
    char tick_string[32] = {0};
    ui2a(ticks, 10, tick_string);
    strcpy(sendBuf + 1, tick_string);

    char replyBuf[32] = {0};
    int response = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, replyBuf, strlen(replyBuf));

    if (response < 0) {  // if Send() cannot reach the TID
        return -1;
    }

    unsigned int time = 0;
    a2ui(replyBuf, 10, &time);
    return time;
}

}  // namespace clock_server
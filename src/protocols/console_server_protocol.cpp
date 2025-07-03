#include "console_server_protocol.h"

#include <cstdarg>

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

int Measurement(int tid, int channel, const char* str) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::MEASUREMENT);
    strcpy(sendBuf + 1, str);

    char reply = 0;
    int response = sys::Send(tid, sendBuf, strlen(str) + 2, &reply, 1);

    if (response < 0) {  // if Send() cannot reach the TID
        return response;
    } else {
        return 0;
    }
}

int Printf(uint32_t tid, const char* fmt, ...) {
    va_list va;
    char buf[12];
    char out[Config::MAX_MESSAGE_LENGTH];
    unsigned int outPos = 0;

    va_start(va, fmt);
    while (*fmt && outPos < sizeof(out) - 1) {
        if (*fmt != '%') {
            out[outPos++] = *fmt++;
            continue;
        }

        fmt++;  // skip '%'

        if (*fmt == '\0') {
            break;
        }

        int width = 0;
        if (*fmt >= '0' && *fmt <= '9') {
            width = a2d(*fmt++);
        }

        const char* str = buf;
        switch (*fmt) {
            case 'u':
                ui2a(va_arg(va, unsigned int), 10, buf);
                break;
            case 'd':
                i2a(va_arg(va, int), buf);
                break;
            case 'x':
                ui2a(va_arg(va, unsigned int), 16, buf);
                break;
            case 'c':
                buf[0] = (char)va_arg(va, int);
                buf[1] = '\0';
                break;
            case 's':
                str = va_arg(va, const char*);
                break;
            case '%':
                buf[0] = '%';
                buf[1] = '\0';
                break;
            default:
                buf[0] = *fmt;
                buf[1] = '\0';
                break;
        }

        fmt++;

        int len = strlen(str);
        for (int i = len; i < width && outPos < sizeof(out) - 1; i++) {
            out[outPos++] = ' ';
        }

        while (*str && outPos < sizeof(out) - 1) {
            out[outPos++] = *str++;
        }
    }

    out[outPos] = '\0';
    va_end(va);

    return Puts(tid, 0, out);
}

}  // namespace console_server
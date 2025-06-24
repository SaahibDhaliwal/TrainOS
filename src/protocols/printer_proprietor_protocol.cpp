#include "printer_proprietor_protocol.h"

#include <cstdarg>

#include "config.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "util.h"

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

void printC(int tid, int channel, const char ch) {
    char sendBuf[3] = {0};
    sendBuf[0] = toByte(Command::PRINTC);
    sendBuf[1] = ch;
    if (ch == '\0') return;  // don't send a char if it's just the end of a string

    char reply = 0;
    int response = sys::Send(tid, sendBuf, 3, &reply, 1);

    ASSERT(response >= 0);
}

void printS(int tid, int channel, const char* str) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::PRINTS);
    strcpy(sendBuf + 1, str);

    char reply = 0;
    int response = sys::Send(tid, sendBuf, strlen(str) + 2, &reply, 1);

    ASSERT(response >= 0);
}

void printF(uint32_t tid, const char* fmt, ...) {
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

    printS(tid, 0, out);
}

// whatever task calls this, will block because of the getIdleTime() sys called
void refreshClocks(int tid) {
    char sendBuf[23] = {0};
    sendBuf[0] = toByte(Command::REFRESH_CLOCKS);
    ui2a(sys::GetIdleTime(), 10, sendBuf + 1);
    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void commandFeedback(command_server::Reply reply, int tid) {
    char sendBuf[3] = {0};
    sendBuf[0] = toByte(Command::COMMAND_FEEDBACK);
    sendBuf[1] = toByte(reply);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void clearCommandPrompt(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::CLEAR_COMMAND_PROMPT);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void backspace(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::BACKSPACE);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTurnout(Command_Byte command, unsigned int turnoutIdx, int tid) {
    ASSERT(command == Command_Byte::SWITCH_STRAIGHT || command == Command_Byte::SWITCH_CURVED,
           "INVALID TURNOUT COMMAND!\r\n");

    char sendBuf[24] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TURNOUT);
    sendBuf[1] = command;

    ui2a(turnoutIdx, 10, sendBuf + 2);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateSensor(char sensorBox, unsigned int sensorNum, int tid) {
    char sendBuf[24] = {0};
    sendBuf[0] = toByte(Command::UPDATE_SENSOR);
    sendBuf[1] = sensorBox;

    ui2a(sensorNum, 10, sendBuf + 2);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void startupPrint(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::STARTUP_PRINT);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

}  // namespace printer_proprietor
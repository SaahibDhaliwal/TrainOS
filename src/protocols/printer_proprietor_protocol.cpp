#include "printer_proprietor_protocol.h"

#include <cstdarg>

#include "config.h"
#include "generic_protocol.h"
#include "ring_buffer.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "util.h"
#include "zone.h"

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

int formatToBuffer(char* out, int outSize, const char* fmt, va_list va) {
    char buf[24];
    unsigned int outPos = 0;

    while (*fmt && outPos < outSize - 1) {
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
        // 127131
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
        for (int i = len; i < width && outPos < outSize - 1; i++) {
            out[outPos++] = ' ';
        }

        while (*str && outPos < outSize - 1) {
            out[outPos++] = *str++;
        }
    }

    out[outPos] = '\0';
    return (outPos < outSize - 1 ? (int)outPos : -1);
}

int formatToString(char* buff, int buffSize, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int len = formatToBuffer(buff, buffSize, fmt, va);
    va_end(va);
    return len;
}

void printF(uint32_t tid, const char* fmt, ...) {
    char out[Config::MAX_MESSAGE_LENGTH];
    va_list va;
    va_start(va, fmt);
    int len = formatToBuffer(out, Config::MAX_MESSAGE_LENGTH, fmt, va);
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

void updateTurnout(int tid, Command_Byte command, unsigned int turnoutIdx) {
    ASSERT(command == Command_Byte::SWITCH_STRAIGHT || command == Command_Byte::SWITCH_CURVED,
           "INVALID TURNOUT COMMAND!\r\n");

    char sendBuf[24] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TURNOUT);
    sendBuf[1] = command;

    ui2a(turnoutIdx, 10, sendBuf + 2);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateSensor(int tid, Sensor sensor, int64_t timeDiff, int64_t distDiff) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_SENSOR);

    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u #Time Diff:%4d ms   Distance Diff:%4d mm",
                   sensor.box, sensor.num, timeDiff, distDiff);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void startupPrint(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::STARTUP_PRINT);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainVelocity(int tid, int trainIndex, uint64_t velocity) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_VELOCITY);
    unsigned int decimal = velocity % 1000;
    unsigned int integer = velocity / 1000;
    // formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u      mm/s", trainIndex + 1, velocity / 1000,
    if (decimal < 10 && integer < 10) {  // two digits
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u     mm/s ", trainIndex + 1,
                       velocity / 1000, decimal);
    } else if ((decimal < 100 && integer < 10) || (decimal < 10 && integer < 100)) {  // three digits
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u    mm/s ", trainIndex + 1, integer,
                       decimal);
    } else if ((decimal < 100 && integer < 1000) || (decimal < 1000 && integer < 100)) {  // four digits
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u   mm/s ", trainIndex + 1, integer,
                       decimal);
    } else if ((decimal < 1000 && integer < 10000) || (decimal < 10000 && integer < 1000)) {  // five digits
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u  mm/s", trainIndex + 1, integer, decimal);
    } else {  // six or more digits
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u.%u mm/s", trainIndex + 1, integer, decimal);
    }

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainDistance(int tid, int trainIndex, uint64_t distance) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_DISTANCE);
    trainIndex++;

    // some silly checking. I'm not proud of this
    if (distance < 0) {
        if (distance < -1000 && distance > -10000) {  // three digits plus negative
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d mm", trainIndex, distance);
        } else if (distance < -100) {  // three total
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d  mm", trainIndex, distance);
        } else if (distance < -10) {
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d   mm", trainIndex, distance);
        } else {  // does this ever happen?
            return;
        }

    } else if (distance < 10) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u    mm", trainIndex, distance);
    } else if (distance < 100) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u   mm", trainIndex, distance);
    } else if (distance < 1000) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u  mm", trainIndex, distance);
    } else {  // does this ever happen?
        return;
    }

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainZoneDistance(int tid, int trainIndex, uint64_t distance) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_ZONE_DISTANCE);
    trainIndex++;

    // some silly checking. I'm not proud of this
    if (distance < 0) {
        if (distance < -1000 && distance > -10000) {  // three digits plus negative
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d mm", trainIndex, distance);
        } else if (distance < -100) {  // three total
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d  mm", trainIndex, distance);
        } else if (distance < -10) {
            formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%d   mm", trainIndex, distance);
        } else {  // does this ever happen?
            return;
        }

    } else if (distance < 10) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u    mm", trainIndex, distance);
    } else if (distance < 100) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u   mm", trainIndex, distance);
    } else if (distance < 1000) {
        formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%u  mm", trainIndex, distance);
    } else {  // does this ever happen?
        return;
    }

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainStatus(int tid, int trainIndex, bool isActive) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_STATUS);
    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c.", trainIndex + 1,
                   isActive);  // period so "false" doesn't concat our string
    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainNextSensor(int tid, int trainIndex, Sensor sensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_SENSOR);

    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%u  ", trainIndex + 1, sensor.box, sensor.num);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainZoneSensor(int tid, int trainIndex, Sensor sensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_ZONE_SENSOR);

    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%u ", trainIndex + 1, sensor.box, sensor.num);

    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void updateTrainZone(int tid, int trainIndex, RingBuffer<ZoneExit, 16> zoneExits) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_TRAIN_ZONE);
    sendBuf[1] = static_cast<char>(trainIndex + 1);

    int strSize = 2;

    for (auto it = zoneExits.begin(); it != zoneExits.end(); ++it) {
        printer_proprietor::formatToString(sendBuf + strSize, Config::MAX_MESSAGE_LENGTH - strSize - 1, "%u ",
                                           it->zoneNum);
        strSize += strlen(sendBuf);
    }

    sys::Send(tid, sendBuf, strSize + 1, nullptr, 0);
}

void updateTrainStatus(int tid, const char* srcName, const char* dstName, const uint64_t microsDeltaT,
                       const uint64_t mmDeltaT) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::MEASUREMENT);

    uint64_t ms = microsDeltaT / 1000;
    uint64_t micros = microsDeltaT % 1000;

    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%s,%s,%u.%u,%u", srcName, dstName, ms, micros,
                   mmDeltaT);
    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void measurementOutput(int tid, const char* srcName, const char* dstName, const uint64_t microsDeltaT,
                       const uint64_t mmDeltaT) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::MEASUREMENT);

    uint64_t ms = microsDeltaT / 1000;
    uint64_t micros = microsDeltaT % 1000;

    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%s,%s,%u.%u,%u", srcName, dstName, ms, micros,
                   mmDeltaT);
    sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
}

void debug(int tid, const char* str) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::DEBUG);
    formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%s", str);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void debugPrintF(int tid, const char* fmt, ...) {
    char out[Config::MAX_MESSAGE_LENGTH];
    out[0] = toByte(Command::DEBUG);
    va_list va;
    va_start(va, fmt);
    int len = formatToBuffer(out + 1, Config::MAX_MESSAGE_LENGTH - 1, fmt, va);
    va_end(va);

    int res = sys::Send(tid, out, strlen(out) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

}  // namespace printer_proprietor
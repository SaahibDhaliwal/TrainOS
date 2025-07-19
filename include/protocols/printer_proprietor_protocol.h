#ifndef __PRINTER_PROPRIETOR_PROTOCOL__
#define __PRINTER_PROPRIETOR_PROTOCOL__

#include <cstdint>

#include "command.h"
#include "command_server_protocol.h"
#include "sensor.h"

constexpr const char* PRINTER_PROPRIETOR_NAME = "printer_proprietor";

namespace printer_proprietor {
enum class Command : char {
    PRINTC,
    PRINTS,
    REFRESH_CLOCKS,
    CLEAR_COMMAND_PROMPT,
    BACKSPACE,
    STARTUP_PRINT,
    COMMAND_FEEDBACK,
    UPDATE_TURNOUT,
    UPDATE_SENSOR,
    UPDATE_TRAIN_VELOCITY,
    UPDATE_TRAIN_STATUS,
    UPDATE_TRAIN_DISTANCE,
    UPDATE_TRAIN_SENSOR,
    UPDATE_TRAIN_ZONE,
    MEASUREMENT,
    DEBUG,
    KILL,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void printS(int tid, int channel, const char* str);
void printC(int tid, int channel, const char ch);
void printF(uint32_t tid, const char* fmt, ...);
void refreshClocks(int tid);
void commandFeedback(command_server::Reply reply, int tid);
void clearCommandPrompt(int tid);
void backspace(int tid);
void updateTurnout(int tid, Command_Byte command, unsigned int turnoutIdx);
void updateSensor(int tid, Sensor sensor, int64_t lastEstimate, int64_t nextSample);

void startupPrint(int tid);
void measurementOutput(int tid, const char* srcName, const char* dstName, const uint64_t microsDeltaT,
                       const uint64_t mmDeltaD);
int formatToString(char* buff, int buffSize, const char* fmt, ...);
void debug(int tid, const char* str);
void debugPrintF(int tid, const char* fmt, ...);

// trains
void updateTrainStatus(int tid, int trainIndex, bool isActive);
void updateTrainVelocity(int tid, int trainIndex, uint64_t velocity);
void updateTrainDistance(int tid, int trainIndex, uint64_t distance);
void updateTrainNextSensor(int tid, int trainIndex, Sensor sensor);
void updateTrainZone(int tid, int trainIndex, const char* zoneString);
}  // namespace printer_proprietor

#endif
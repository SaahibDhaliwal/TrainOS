#ifndef __PRINTER_PROPRIETOR_PROTOCOL__
#define __PRINTER_PROPRIETOR_PROTOCOL__

#include <cstdint>

#include "command.h"
#include "command_server_protocol.h"

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
    UPDATE_TRAIN,
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
void updateSensor(int tid, char sensorBox, unsigned int sensorNum);
void updateTrainStatus(int tid, char sensorBox, unsigned int sensorNum);
void startupPrint(int tid);

}  // namespace printer_proprietor

#endif
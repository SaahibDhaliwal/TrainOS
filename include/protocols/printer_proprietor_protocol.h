#ifndef __PRINTER_PROPRIETOR_PROTOCOL__
#define __PRINTER_PROPRIETOR_PROTOCOL__

#include <cstdint>

namespace printer_proprietor {
enum class Command : char { PRINTC, PRINTS, REFRESH_CLOCKS, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void printS(int tid, int channel, const char* str);
void printC(int tid, int channel, const char ch);
void printF(uint32_t tid, const char* fmt, ...);
void refreshClocks(int tid);

}  // namespace printer_proprietor

#endif
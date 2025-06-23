#ifndef __CONSOLE_SERVER_PROTOCOL__
#define __CONSOLE_SERVER_PROTOCOL__
#include <cstdint>

namespace console_server {
enum class Command : char { GET, PUT, PUTS, TX, RX, RT, TX_CONNECT, RX_CONNECT, PRINT, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Getc(int tid, int channel);

int Putc(int tid, int channel, unsigned char ch);

int Puts(int tid, int channel, const char* str);

int Printf(uint32_t tid, const char* fmt, ...);
}  // namespace console_server

#endif
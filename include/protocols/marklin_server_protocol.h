#ifndef __MARKLIN_SERVER_PROTOCOL__
#define __MARKLIN_SERVER_PROTOCOL__

namespace marklin_server {
enum class Command : char { GET, PUT, PUTS, TX, RX, CTS, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Getc(int tid, int channel);

int Putc(int tid, int channel, unsigned char ch);

int Puts(int tid, int channel, const char* str);

}  // namespace marklin_server

#endif
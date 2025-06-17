#ifndef __CO_PROTOCOL__
#define __CO_PROTOCOL__

namespace console_server {
enum class Command : char { GET, PUT, TX, RX, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Getc(int tid, int channel);

int Putc(int tid, int channel, unsigned char ch);

}  // namespace console_server

#endif
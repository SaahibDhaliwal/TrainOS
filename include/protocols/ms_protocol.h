#ifndef __MS_PROTOCOL__
#define __MS_PROTOCOL__

namespace marklin_server {
enum class Command : char { GET, PUT, TX, RX, CTS, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Getc(int tid, int channel);

int Putc(int tid, int channel, unsigned char ch);

}  // namespace marklin_server

#endif
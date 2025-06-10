#ifndef __CS_PROTOCOL__
#define __CS_PROTOCOL__

namespace clock_server {
enum class Command : char { TIME, DELAY, DELAY_UNTIL, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);
}  // namespace clock_server

#endif
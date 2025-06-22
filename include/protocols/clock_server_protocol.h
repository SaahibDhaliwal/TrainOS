#ifndef __CLOCK_SERVER_PROTOCOL__
#define __CLOCK_SERVER_PROTOCOL__

namespace clock_server {
enum class Command : char { TIME, DELAY, DELAY_UNTIL, KILL, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { FAILURE, SUCCESS, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Time(int tid);

int Delay(int tid, int ticks);

int DelayUntil(int tid, int ticks);

}  // namespace clock_server

#endif
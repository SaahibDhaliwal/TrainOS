#ifndef __COMMAND_SERVER_PROTOCOL__
#define __COMMAND_SERVER_PROTOCOL__

namespace command_server {
enum class Command : char { MESSAGE, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { FAILURE, SUCCESS, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

}  // namespace command_server

#endif
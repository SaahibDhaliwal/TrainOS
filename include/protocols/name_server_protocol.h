#ifndef __NAME_SERVER_PROTOCOL__
#define __NAME_SERVER_PROTOCOL__

namespace name_server {
enum class Command : char { REGISTER, WHO_IS, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { FAILURE, SUCCESS, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int RegisterAs(const char *name);

int WhoIs(const char *name);

}  // namespace name_server

#endif

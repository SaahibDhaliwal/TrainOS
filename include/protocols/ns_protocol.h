#ifndef __NS_PROTOCOL__
#define __NS_PROTOCOL__

namespace name_server {
enum class Command : char { REGISTER, WHO_IS, COUNT, UNKNOWN_COMMAND };
char toByte(Command c);
Command commandFromByte(char c);

int RegisterAs(const char *name);

int WhoIs(const char *name);

}  // namespace name_server

#endif

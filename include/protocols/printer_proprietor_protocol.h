#ifndef __PRINTER_PROPRIETOR_PROTOCOL__
#define __PRINTER_PROPRIETOR_PROTOCOL__

namespace printer_proprietor {
enum class Command : char { PRINTS, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void printS(const char* str);
void updateSwitch();

}  // namespace printer_proprietor

#endif
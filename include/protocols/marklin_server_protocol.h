#ifndef __MARKLIN_SERVER_PROTOCOL__
#define __MARKLIN_SERVER_PROTOCOL__

constexpr const char* MARKLIN_SERVER_NAME = "marklin_server";

namespace marklin_server {

enum class Command : char {
    GET,
    PUT,
    PUTS,
    TX,
    RX,
    CTS,
    TX_CONNECT,
    RX_CONNECT,
    CTS_CONNECT,
    KILL,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

int Getc(int tid, int channel);

int Putc(int tid, int channel, unsigned char ch);

int Puts(int tid, int channel, const char* str);

int setTrainSpeed(int tid, unsigned int trainSpeed, unsigned int trainNumber);

int setTrainReverse(int tid, unsigned int trainNumber);

int setTrainReverseAndSpeed(int tid, unsigned int trainSpeed, unsigned int trainNumber);

int setTurnout(int tid, unsigned int turnoutDirection, unsigned int turnoutNumber);

int solenoidOff(int tid);

void reverseTrainTask();

}  // namespace marklin_server

#endif
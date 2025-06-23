#ifndef __MARKLIN_COMMAND_PROTOCOL__
#define __MARKLIN_COMMAND_PROTOCOL__

namespace marklin_command {
enum class Command : char { GET, PUT, PUTS, TX, RX, CTS, TX_CONNECT, RX_CONNECT, CTS_CONNECT, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

enum class Sensor_Bank : char {
    A = 0x41,
    B,
    C,
    D,
    E,
};

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void setTrainSpeed(int trainId, int speed);

void reverseTrain(int train, int speed);

void SensorServer();

}  // namespace marklin_command

#endif
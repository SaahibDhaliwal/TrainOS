#ifndef __MARKLIN_COMMAND_PROTOCOL__
#define __MARKLIN_COMMAND_PROTOCOL__

namespace marklin_command {
enum class Command : char { GET, PUT, PUTS, TX, RX, CTS, TX_CONNECT, RX_CONNECT, CTS_CONNECT, COUNT, UNKNOWN_COMMAND };
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

enum class Command_Byte : unsigned char {
    TRAIN_STOP = 0x00 + 16,
    TRAIN_SPEED_1,
    TRAIN_SPEED_2,
    TRAIN_SPEED_3,
    TRAIN_SPEED_4,
    TRAIN_SPEED_5,
    TRAIN_SPEED_6,
    TRAIN_SPEED_7,
    TRAIN_SPEED_8,
    TRAIN_SPEED_9,
    TRAIN_SPEED_10,
    TRAIN_SPEED_11,
    TRAIN_SPEED_12,
    TRAIN_SPEED_13,
    TRAIN_SPEED_14,
    TRAIN_REVERSE,
    SOLENOID_OFF = 0x20,
    SWITCH_STRAIGHT,
    SWITCH_CURVED,
    SENSOR_READ_ALL = 0x85,
};

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
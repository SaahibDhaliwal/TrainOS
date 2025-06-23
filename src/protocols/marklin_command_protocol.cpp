#include "marklin_command_protocol.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "config.h"
#include "generic_protocol.h"
#include "idle_time.h"
// #include "intrusive_node.h"
#include "name_server.h"
#include "name_server_protocol.h"
// #include "queue.h"
// #include "ring_buffer.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "util.h"
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

struct SensorResult {
    Sensor_Bank bank;
    uint16_t sensor;
    SensorResult(Sensor_Bank bankInit, uint16_t sensorInit) : bank(bankInit), sensor(sensorInit) {
    }
};

namespace marklin_command {

char toByte(Command c) {
    return static_cast<char>(c);
}

char toByte(Reply r) {
    return static_cast<char>(r);
}

Command commandFromByte(char c) {
    return (c < static_cast<char>(Command::COUNT)) ? static_cast<Command>(c) : Command::UNKNOWN_COMMAND;
}

Reply replyFromByte(char c) {
    return (c < static_cast<char>(Reply::COUNT)) ? static_cast<Reply>(c) : Reply::UNKNOWN_REPLY;
}

}  // namespace marklin_command
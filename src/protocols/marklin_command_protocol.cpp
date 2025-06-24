#include "marklin_command_protocol.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "config.h"
#include "generic_protocol.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "util.h"

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
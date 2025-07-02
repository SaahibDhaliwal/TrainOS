#include "localization_server_protocol.h"

#include "generic_protocol.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "util.h"

namespace localization_server {

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

void setTrainSpeed(int tid, unsigned int trainSpeed, unsigned int trainNumber) {
    char sendBuf[6] = {0};
    sendBuf[0] = toByte(Command::SET_SPEED);

    trainSpeed *= 100;
    trainSpeed += trainNumber;
    ui2a(trainSpeed, 10, sendBuf + 1);
    int res = sys::Send(tid, sendBuf, 5, nullptr, 0);

    handleSendResponse(res, tid);
}

void reverseTrain(int tid, unsigned int trainNumber) {
    char sendBuf[3] = {0};
    sendBuf[0] = toByte(Command::REVERSE_TRAIN);
    ui2a(trainNumber, 10, sendBuf + 1);
    int res = sys::Send(tid, sendBuf, 3, nullptr, 0);
    handleSendResponse(res, tid);
}

void updateSensor(int tid, char box, unsigned int sensorNum) {
    char sendBuf[3] = {0};
    sendBuf[0] = toByte(Command::SENSOR_UPDATE);
    sendBuf[1] = box;
    sendBuf[2] = sensorNum;
    int res = sys::Send(tid, sendBuf, 4, nullptr, 0);
    handleSendResponse(res, tid);
}

}  // namespace localization_server
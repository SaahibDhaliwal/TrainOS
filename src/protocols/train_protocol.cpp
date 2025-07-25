#include "train_protocol.h"

#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "printer_proprietor_protocol.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "util.h"

namespace train_server {

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

void sendSensorInfo(int tid, char currentBox, unsigned int currentSensorNum, char nextBox, unsigned int nextSensorNum,
                    uint64_t distance) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::NEW_SENSOR);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%c%d", currentBox,
                                       static_cast<char>(currentSensorNum), nextBox, static_cast<char>(nextSensorNum),
                                       distance);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void setTrainSpeed(int tid, unsigned int trainSpeed) {
    char sendBuf[6] = {0};
    sendBuf[0] = toByte(Command::SET_SPEED);
    // train speed is always two digits (we add 16) so this is fine
    ui2a(trainSpeed, 10, sendBuf + 1);
    int res = sys::Send(tid, sendBuf, 5, nullptr, 0);
    handleSendResponse(res, tid);
}

void reverseTrain(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::REVERSE_COMMAND);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf), nullptr, 0);
    handleSendResponse(res, tid);
}

unsigned int getReverseDelayTicks(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::GET_REVERSE_TIME);

    char receiveBuffer[21] = {0};  // max digits is 20

    int res = sys::Send(tid, sendBuf, strlen(sendBuf), receiveBuffer, 21);
    handleSendResponse(res, tid);

    unsigned int result = 0;
    a2ui(receiveBuffer, 10, &result);
    return result;
}

void finishReverse(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::FINISH_REVERSE);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf), nullptr, 0);
    handleSendResponse(res, tid);
}

void sendStopInfo(int tid, char currentBox, unsigned int currentSensorNum, char targetBox, unsigned int targetSensorNum,
                  uint64_t offset) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::STOP_SENSOR);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%c%u", currentBox,
                                       currentSensorNum, targetBox, targetSensorNum, offset);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

}  // namespace train_server
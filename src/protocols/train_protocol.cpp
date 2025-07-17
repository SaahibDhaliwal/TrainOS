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
    sendBuf[0] = toByte(Command::NEXT_SENSOR);
    // ASSERT(0, "currentBox: %c currentSensorNum: %d nextBox: %c nextSensorNum: %d", currentBox, currentSensorNum,
    //    nextBox, nextSensorNum);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%c%d", currentBox,
                                       static_cast<char>(currentSensorNum), nextBox, static_cast<char>(nextSensorNum),
                                       distance);
    // ASSERT(0, "formatted string output: sendbuf[1]: %c sendbuf[2]: %d", sendBuf[1], sendBuf[2]);
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
// void setTrainSpeed(int tid, char* trainSpeed) {
//     char sendBuf[2] = {0};
//     sendBuf[0] = toByte(Command::SET_SPEED);
//     sendBuf[1] = trainSpeed[0];
//     sendBuf[2] = trainSpeed[1];
//     int res = sys::Send(tid, sendBuf, strlen(sendBuf), nullptr, 0);  // is this bad?
//     handleSendResponse(res, tid);
// }

}  // namespace train_server
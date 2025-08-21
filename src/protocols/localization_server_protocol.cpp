#include "localization_server_protocol.h"

#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "train_protocol.h"
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
    // train speed is always two digits (we add 16) so this is fine
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
    ui2a(sensorNum, 10, sendBuf + 2);
    // sendBuf[2] = sensorNum;
    int res = sys::Send(tid, sendBuf, 4, nullptr, 0);
    handleSendResponse(res, tid);
}

void setTurnout(int tid, unsigned int turnoutDirection, unsigned int turnoutNumber) {
    char sendBuf[3] = {0};
    sendBuf[0] = toByte(Command::SET_TURNOUT);
    sendBuf[1] = static_cast<char>(turnoutDirection);
    sendBuf[2] = static_cast<char>(turnoutNumber);
    int res = sys::Send(tid, sendBuf, 4, nullptr, 0);
    handleSendResponse(res, tid);
}

void solenoidOff(int tid) {
    char sendBuf[2] = {0};
    sendBuf[0] = toByte(Command::SOLENOID_OFF);
    sendBuf[1] = static_cast<char>(Command_Byte::SOLENOID_OFF);
    int res = sys::Send(tid, sendBuf, 3, nullptr, 0);
    handleSendResponse(res, tid);
}

// Note: I'm passing the sensor num and train num as a char.
void setStopLocation(int tid, int trainNumber, char box, int sensorNum, int offset) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::SET_STOP);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%d",
                                       static_cast<char>(trainNumber), box, sensorNum, offset);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void resetTrack(int tid) {
    char sendBuf = toByte(Command::RESET_TRACK);
    int res = sys::Send(tid, &sendBuf, 1, nullptr, 0);
    handleSendResponse(res, tid);
}

UpdateResInfo updateReservation(int tid, int trainIndex, RingBuffer<ReservedZone, 32> reservedZones,
                                ReservationType reservation) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_RESERVATION);
    sendBuf[1] = static_cast<char>(trainIndex + 1);
    sendBuf[2] = toByte(reservation);

    char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};

    int strSize = 3;

    for (auto it = reservedZones.begin(); it != reservedZones.end(); ++it) {
        printer_proprietor::formatToString(sendBuf + strSize, Config::MAX_MESSAGE_LENGTH - strSize - 1, "%c%c",
                                           it->sensorMarkingEntrance.box, it->sensorMarkingEntrance.num);
        strSize += strlen(sendBuf + strSize);
    }

    int res = sys::Send(tid, sendBuf, strSize + 1, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

    handleSendResponse(res, tid);

    if (receiveBuff[0] == 'T') {
        Sensor stopSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
        Sensor targetSensor{.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};
        Sensor firstSensor{.box = receiveBuff[5], .num = static_cast<uint8_t>(receiveBuff[6])};
        bool reverse = receiveBuff[7] == 't';
        unsigned int distance = 0;
        a2ui(&receiveBuff[8], 10, &distance);
        return UpdateResInfo{.destInfo = DestinationInfo{.stopSensor = stopSensor,
                                                         .targetSensor = targetSensor,
                                                         .firstSensor = firstSensor,
                                                         .distance = distance,
                                                         .reverse = reverse},
                             .hasNewPath = true};
    }

    return UpdateResInfo{.hasNewPath = false};
}

DestinationInfo newDestination(int tid, int trainIndex) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuf[0] = toByte(Command::NEW_DESTINATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c", trainIndex + 1);

    char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

    handleSendResponse(res, tid);

    Sensor stopSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
    Sensor targetSensor{.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};
    Sensor firstSensor{.box = receiveBuff[5], .num = static_cast<uint8_t>(receiveBuff[6])};
    bool reverse = receiveBuff[7] == 't';
    unsigned int distance = 0;
    a2ui(&receiveBuff[8], 10, &distance);

    return DestinationInfo{.stopSensor = stopSensor,
                           .targetSensor = targetSensor,
                           .firstSensor = firstSensor,
                           .distance = distance,
                           .reverse = reverse};
}

void initPlayer(int tid, int trainIndex, Sensor initSensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuf[0] = toByte(Command::INIT_PLAYER);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       initSensor.box, initSensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void initTrain(int tid, int trainIndex, Sensor initSensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuf[0] = toByte(Command::INIT_TRAIN);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       initSensor.box, initSensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void playerInput(int tid, char input) {
    // our allowed player controls (r for reverse, q to quit)
    if (input != 'w' && input != 'a' && input != 's' && input != 'd' && input != 'r' && input != 'q') {
        return;
    }

    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::PLAYER_INPUT);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c", input);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void courierSendSensorInfo(int tid, char currentBox, unsigned int currentSensorNum, char nextBox,
                           unsigned int nextSensorNum) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(train_server::Command::NEW_SENSOR);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%c", currentBox,
                                       static_cast<char>(currentSensorNum), nextBox, static_cast<char>(nextSensorNum));
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void TrainSensorCourier() {
    int parentTid = sys::MyParentTid();
    ASSERT(parentTid != -1, "SENSOR COURIER UNABLE TO GET PARENT \r\n", sys::MyTid());

    uint32_t clientTid = 0;
    // ensure we send the index after spawning
    int trainTid = uIntReceive(&clientTid);
    emptyReply(clientTid);

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        emptyReply(clientTid);
        sys::Send(trainTid, receiveBuffer, strlen(receiveBuffer), nullptr, 0);  // pass along the message
    }
}

}  // namespace localization_server
#include "localization_server_protocol.h"

#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "printer_proprietor_protocol.h"
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

// either returns your zone num (success) or zero (failure)
void makeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    // char replyBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::MAKE_RESERVATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void initReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    // char replyBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::INITIAL_RESERVATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void freeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuf[0] = toByte(Command::FREE_RESERVATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void updateReservation(int tid, int trainIndex, RingBuffer<ReservedZone, 32> reservedZones,
                       ReservationType reservation) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_RESERVATION);
    sendBuf[1] = static_cast<char>(trainIndex + 1);
    sendBuf[2] = toByte(reservation);

    int strSize = 3;

    for (auto it = reservedZones.begin(); it != reservedZones.end(); ++it) {
        printer_proprietor::formatToString(sendBuf + strSize, Config::MAX_MESSAGE_LENGTH - strSize - 1, "%c%c",
                                           it->sensorMarkingEntrance.box, it->sensorMarkingEntrance.num);
        strSize += strlen(sendBuf + strSize);
    }

    int res = sys::Send(tid, sendBuf, strSize + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void newDestination(int tid, int trainIndex) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuf[0] = toByte(Command::NEW_DESTINATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c", trainIndex + 1);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

}  // namespace localization_server
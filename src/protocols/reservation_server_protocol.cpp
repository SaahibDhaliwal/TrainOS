#include "reservation_server_protocol.h"

#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "util.h"

namespace reservation_server {

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

// either returns your zone num (success) or zero (failure)
void makeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff) {
    char sendBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuff[0] = toByte(Command::MAKE_RESERVATION);
    printer_proprietor::formatToString(sendBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuff, strlen(sendBuff) + 1, replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void freeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff) {
    char sendBuff[Config::MAX_MESSAGE_LENGTH] = {0};

    sendBuff[0] = toByte(Command::FREE_RESERVATION);
    printer_proprietor::formatToString(sendBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c", trainIndex + 1,
                                       sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuff, strlen(sendBuff) + 1, replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void updateReservation(int tid, int trainIndex, char* sendBuff, char* replyBuff) {
    sendBuff[1] = static_cast<char>(trainIndex + 1);
    int res = sys::Send(tid, sendBuff, strlen(sendBuff), replyBuff, Config::MAX_MESSAGE_LENGTH);
    handleSendResponse(res, tid);
}

void ReservationCourier() {
    int parentTid = sys::MyParentTid();
    ASSERT(parentTid != -1, "RESERVATION COURIER UNABLE TO GET PARENT \r\n", sys::MyTid());

    int reservationServerTid = name_server::WhoIs(RESERVATION_SERVER_NAME);
    ASSERT(reservationServerTid >= 0, "UNABLE TO GET RESERVATION_SERVER_NAME\r\n");

    uint32_t clientTid = 0;
    int trainIndex = uIntReceive(&clientTid) - 1;
    emptyReply(clientTid);

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        emptyReply(clientTid);
        char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        Command command = commandFromByte(receiveBuffer[0]);
        // note: these will have the first byte be a reply byte in a send message. Will need to check on train's side
        switch (command) {
            case Command::MAKE_RESERVATION: {
                // is this not the same cyclic deadlock?
                // where the train might call "courier make reservation" twice really quickly
                // we need to prevent train from using any courier service until they have returned
                Sensor reservationSensor = {.box = receiveBuffer[1], .num = static_cast<uint8_t>(receiveBuffer[2])};
                makeReservation(reservationServerTid, trainIndex, reservationSensor, replyBuff);
                sys::Send(parentTid, replyBuff, strlen(replyBuff), nullptr, 0);  // pass along the message
                break;
            }
            case Command::FREE_RESERVATION: {
                Sensor freeSensor = {.box = receiveBuffer[1], .num = static_cast<uint8_t>(receiveBuffer[2])};
                freeReservation(reservationServerTid, trainIndex, freeSensor, replyBuff);
                sys::Send(parentTid, replyBuff, strlen(replyBuff), nullptr, 0);  // pass along the message
                break;
            }
            case Command::UPDATE_RESERVATION: {
                updateReservation(reservationServerTid, trainIndex, receiveBuffer, replyBuff);
                sys::Send(parentTid, replyBuff, strlen(replyBuff), nullptr, 0);  // pass along the message
                break;
            }
            default:
                ASSERT(0, "bad command");
        }
    }
}

void courierMakeReservation(int tid, Sensor sensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::MAKE_RESERVATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c", sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void courierFreeReservation(int tid, Sensor sensor) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::FREE_RESERVATION);
    printer_proprietor::formatToString(sendBuf + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c", sensor.box, sensor.num);
    int res = sys::Send(tid, sendBuf, strlen(sendBuf) + 1, nullptr, 0);
    handleSendResponse(res, tid);
}

void courierUpdateReservation(int tid, RingBuffer<ReservedZone, 32> reservedZones, ReservationType reservation) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH] = {0};
    sendBuf[0] = toByte(Command::UPDATE_RESERVATION);
    sendBuf[1] = 1;  // some placeholder number to change in the courier
    sendBuf[2] = toByte(reservation);

    int strSize = 3;

    for (auto it = reservedZones.begin(); it != reservedZones.end(); ++it) {
        printer_proprietor::formatToString(sendBuf + strSize, Config::MAX_MESSAGE_LENGTH - strSize - 1, "%c%c",
                                           it->sensorMarkingEntrance.box, it->sensorMarkingEntrance.num);
        strSize += strlen(sendBuf + strSize);
    }

    int res = sys::Send(tid, sendBuf, strSize + 1, nullptr, Config::MAX_MESSAGE_LENGTH - 1);

    handleSendResponse(res, tid);
}

}  // namespace reservation_server
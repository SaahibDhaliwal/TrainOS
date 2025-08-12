#include "reservation_server.h"

#include <utility>

#include "clock_server_protocol.h"
#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "reservation_server_protocol.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "track_data.h"

using namespace reservation_server;

void ReservationServer() {
    int registerReturn = name_server::RegisterAs(RESERVATION_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    TrackNode track[TRACK_MAX];
#if defined(TRACKA)
    init_tracka(track);
#else
    init_trackb(track);
#endif
    TrainReservation trainReservation;
    trainReservation.initialize(track, printerProprietorTid);

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);

        Command command = commandFromByte(receiveBuffer[0]);
        char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        switch (command) {
            case Command::MAKE_RESERVATION: {
                int trainIndex = (receiveBuffer[1]) - 1;
                Sensor reservationSensor = {.box = receiveBuffer[2], .num = static_cast<uint8_t>(receiveBuffer[3])};
                int targetTrackNodeIdx = trackNodeIdxFromSensor(reservationSensor);
                ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");
                TrackNode* reservationTrackNode = &track[targetTrackNodeIdx];
                uint32_t zoneNum = trainReservation.trackNodeToZoneNum(reservationTrackNode);
                if (trainReservation.reservationAttempt(reservationTrackNode, trainIndexToNum(trainIndex))) {
                    replyBuff[0] = toByte(Reply::RESERVATION_SUCCESS);
                    printer_proprietor::formatToString(replyBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c", zoneNum);
                    sys::Reply(clientTid, replyBuff, strlen(replyBuff));
                    break;
                } else if (trainReservation.reservationStatus(reservationTrackNode) == ReservationType::STOPPED) {
                    replyBuff[0] = toByte(Reply::STOPPED_TRAIN_IN_ZONE);
                } else {
                    replyBuff[0] = toByte(Reply::RESERVATION_FAIL);
                }
                // will return which train is there
                uint32_t trainNum = trainReservation.isSectionReserved(reservationTrackNode);
                ASSERT(trainNum != 0);
                printer_proprietor::formatToString(replyBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c", zoneNum,
                                                   trainNum);
                sys::Reply(clientTid, replyBuff, strlen(replyBuff));
                break;
            }
            case Command::FREE_RESERVATION: {
                int trainIndex = receiveBuffer[1] - 1;
                Sensor zoneExitSensor{.box = receiveBuffer[2], .num = receiveBuffer[3]};
                int targetTrackNodeIdx = trackNodeIdxFromSensor(zoneExitSensor);
                ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");
                // why was this the reverse?
                bool result =
                    trainReservation.freeReservation(track[targetTrackNodeIdx].reverse, trainIndexToNum(trainIndex));
                replyBuff[1] = trainReservation.trackNodeToZoneNum(track[targetTrackNodeIdx].reverse);
                if (result) {
                    replyBuff[0] = toByte(Reply::FREE_SUCCESS);
                } else {
                    replyBuff[0] = toByte(Reply::FREE_FAIL);
                }
                sys::Reply(clientTid, replyBuff, strlen(replyBuff));
                break;  // just to surpress compilation warning
            }
            case Command::UPDATE_RESERVATION: {
                int trainIndex = receiveBuffer[1] - 1;
                int buffLen = strlen(receiveBuffer);
                ReservationType rType = reservationFromByte(receiveBuffer[2]);
                for (int i = 3; i + 1 <= buffLen; i += 2) {
                    Sensor reservedZoneSensor{.box = receiveBuffer[i], .num = receiveBuffer[i + 1]};
                    int targetTrackNodeIdx = trackNodeIdxFromSensor(reservedZoneSensor);
                    TrackNode* sensorNode = &track[targetTrackNodeIdx];
                    trainReservation.updateReservation(sensorNode, trainIndexToNum(trainIndex), rType);
                }
                emptyReply(clientTid);
                break;
            }
            default:
                ASSERT(0, "bad command passed");
        }
    }
}
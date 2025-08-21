#include "train_task.h"

#include <algorithm>
#include <utility>

#include "clock_server_protocol.h"
#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "localization_server_protocol.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "pathfinding.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "reservation_server_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "train.h"
#include "train_protocol.h"
#include "util.h"

void TrainUpdater() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "TRAIN NOTIFIER: UNABLE TO GET CLOCK_SERVER_NAME\r\n");
    int parentTid = sys::MyParentTid();
    int delayAmount = 3;  // should there be some desync across notifiers?
    for (;;) {
        int res = emptySend(parentTid);
        handleSendResponse(res, parentTid);
        clock_server::Delay(clockServerTid, delayAmount);
    }
    sys::Exit();
}

void StopTrain() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");
    int parentTid = sys::MyParentTid();
    char replyBuff[8];  // assuming our ticks wont be more than 99999 ticks
    unsigned int delayAmount = 0;
    for (;;) {
        int res = sys::Send(parentTid, nullptr, 0, replyBuff, 7);
        handleSendResponse(res, parentTid);
        a2ui(replyBuff, 10, &delayAmount);
        clock_server::Delay(clockServerTid, delayAmount);
    }
    sys::Exit();
}

using namespace train_server;

void TrainTask() {
    int parentTid = sys::MyParentTid();
    ASSERT(parentTid != -1, "TRAIN %d UNABLE TO GET PARENT \r\n", sys::MyTid());

    uint32_t clientTid = 0;
    unsigned int myTrainNumber = uIntReceive(&clientTid);
    emptyReply(clientTid);

    char sendBuff[40] = {0};
    printer_proprietor::formatToString(sendBuff, 40, "Train_%d", myTrainNumber);
    int registerReturn = name_server::RegisterAs(sendBuff);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER TASK %d\r\n", myTrainNumber);

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "TRAIN NOTIFIER: UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    int updaterTid = sys::Create(Config::NOTIFIER_PRIORITY, TrainUpdater);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == updaterTid, "TRAIN TASK DID NOT RECEIVE FROM NOTIFIER");

    int stopNotifierTid = sys::Create(Config::NOTIFIER_PRIORITY, StopTrain);  // train stopper
    emptyReceive(&clientTid);
    ASSERT(clientTid == stopNotifierTid, "TRAIN TASK DID NOT RECEIVE FROM STOPPER");

    // should we just do this in the initialization?
    // also, what priority should it have?
    int reservationTid = sys::Create(Config::NOTIFIER_PRIORITY, reservation_server::ReservationCourier);
    printer_proprietor::formatToString(sendBuff, Config::MAX_MESSAGE_LENGTH, "%d", trainNumToIndex(myTrainNumber) + 1);
    int res = sys::Send(clientTid, sendBuff, strlen(sendBuff), nullptr, 0);

    Train train(myTrainNumber, parentTid, printerProprietorTid, marklinServerTid, clockServerTid, updaterTid,
                stopNotifierTid, reservationTid);

    printer_proprietor::debugPrintF(printerProprietorTid, "Train %u tid is %u", myTrainNumber, sys::MyTid());

    for (;;) {
        char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        sys::Receive(&clientTid, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

        if (clientTid == parentTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    emptyReply(clientTid);  // immediate reply
                    unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);
                    train.setTrainSpeed(trainSpeed);
                    break;
                }
                case Command::NEW_SENSOR: {
                    Sensor curSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
                    Sensor upcomingSensor = {.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};

                    ASSERT(upcomingSensor.num <= 16 && upcomingSensor.num >= 1, "got some bad sensor num: %u",
                           static_cast<uint8_t>(receiveBuff[4]));
                    ASSERT(curSensor.num <= 16 && curSensor.num >= 1, "got some bad cursensor num: %u",
                           static_cast<uint8_t>(receiveBuff[2]));

                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);
                    emptyReply(clientTid);  // is this reply actually replying?
                    train.processSensorHit(curSensor, upcomingSensor, distance);
                    break;
                }
                case Command::REVERSE_COMMAND: {
                    train.reverseCommand();
                    emptyReply(clientTid);  // reply after finishing reverse (I don't want the path until after)

                    break;
                }
                case Command::STOP_SENSOR: {
                    printer_proprietor::debugPrintF(printerProprietorTid, "RECEIVING STOP INFO AND IMMEDIATE REPLY");

                    emptyReply(clientTid);  // immediate reply

                    printer_proprietor::debugPrintF(printerProprietorTid, "BACK IN STOP SENSOR");

                    Sensor stopSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
                    Sensor targetSensor{.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};
                    Sensor firstSensor{.box = receiveBuff[5], .num = static_cast<uint8_t>(receiveBuff[6])};
                    bool reverse = receiveBuff[7] == 't';

                    printer_proprietor::debugPrintF(printerProprietorTid, "FIRST SENSOR IS: %c%u", firstSensor.box,
                                                    firstSensor.num);

                    unsigned int distance = 0;
                    a2ui(&receiveBuff[8], 10, &distance);

                    train.newStopLocation(stopSensor, targetSensor, firstSensor, distance, reverse);

                    break;
                }
                case Command::INIT_PLAYER: {
                    emptyReply(clientTid);  // immediate reply
                    train.initPlayer();
                    break;
                }
                case Command::INIT_CHASER: {
                    emptyReply(clientTid);  // immediate reply
                    train.initCPU();
                    break;
                }

                default:
                    ASSERT(0, "BAD COMMAND FROM LOCALIZATION SERVER");
                    return;
            }

        } else if (clientTid == updaterTid) {
            if (train.isPlayer) {
                train.updatePlayerState();
            } else {
                train.updateState();
            }
            emptyReply(updaterTid);  // not immediate

        } else if (clientTid == reservationTid) {
            emptyReply(clientTid);
            // deal with it in the train object
            train.handleReservationReply(receiveBuff);

        } else {
            ASSERT(0, "someone else sent to train task?");
        }
    }
}
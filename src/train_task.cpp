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
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "train.h"
#include "train_protocol.h"
#include "util.h"

#define NOTIFIER_PRIORITY 15

void TrainNotifier() {
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

    uint32_t notifierTid = sys::Create(NOTIFIER_PRIORITY, TrainNotifier);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == notifierTid, "train startup received from someone other than the train notifier");

    uint32_t stopNotifierTid = sys::Create(42, StopTrain);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == stopNotifierTid, "train startup received from someone other than the stop train notifier");

    Train train(myTrainNumber, printerProprietorTid, marklinServerTid, clockServerTid);

    for (;;) {
        char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        sys::Receive(&clientTid, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

        if (clientTid == parentTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    emptyReply(clientTid);  // reply right away to reduce latency
                    unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);
                    train.setTrainSpeed(trainSpeed);
                    break;
                }
                case Command::NEW_SENSOR: {
                    emptyReply(clientTid);  // reply right away to reduce latency

                    Sensor curSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
                    Sensor upcomingSensor = {.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};
                    ASSERT(upcomingSensor.num <= 16 && upcomingSensor.num >= 1, "got some bad sensor num: %u",
                           static_cast<uint8_t>(receiveBuff[4]));
                    ASSERT(curSensor.num <= 16 && curSensor.num >= 1, "got some bad sensor num: %u",
                           static_cast<uint8_t>(receiveBuff[2]));

                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);

                    // pass it here
                    train.processSensorHit(curSensor, upcomingSensor, distance);

                    break;
                }
                case Command::REVERSE: {
                    emptyReply(clientTid);  // reply right away to reduce latency
                    train.reverseCommand();
                    break;
                }

                case Command::STOP_SENSOR: {
                    emptyReply(clientTid);  // reply right away to reduce latency
                    Sensor stopSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};
                    Sensor targetSensor{.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};
                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);
                    train.newStopLocation(stopSensor, targetSensor, distance);
                    break;
                }

                default:
                    return;
            }
        } else if (clientTid == notifierTid) {  // so we have some velocity estimate to start with
            train.processQuickNotifier(notifierTid);
        } else if (clientTid == train.reverseTid) {
            Command command = commandFromByte(receiveBuff[0]);
            train.reverseNotifier(command, clientTid);
        } else if (clientTid == stopNotifierTid) {
            train.stopNotifier();
        } else {
            ASSERT(0, "someone else sent to train task?");
        }
    }
}
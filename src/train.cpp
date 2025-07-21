#include "train.h"

#include <algorithm>

#include "command.h"
#include "marklin_server_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "sensor.h"
#include "test_utils.h"

static const int trainAddresses[MAX_TRAINS] = {13, 14, 15, 17, 18, 55};
// train 55 is 10H

static const int trainFastVelocitySeedTrackB[MAX_TRAINS] = {596041, 605833, 596914, 266566, 627366, 525104};
static const int trainStopVelocitySeedTrackB[MAX_TRAINS] = {253549, 257347, 253548, 266566, 266566, 311583};
static const int trainStoppingDistSeedTrackB[MAX_TRAINS] = {400, 400, 400, 400, 400, 400};

static const int trainFastVelocitySeedTrackA[MAX_TRAINS] = {601640, 605833, 596914, 266566, 625911, 525104};
static const int trainStopVelocitySeedTrackA[MAX_TRAINS] = {254904, 257347, 253548, 266566, 265294, 311583};
static const int trainStoppingDistSeedTrackA[MAX_TRAINS] = {400, 400, 400, 400, 410, 400};

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}

int trainIndexToNum(int trainIndex) {
    return trainAddresses[trainIndex];
}

int getFastVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainFastVelocitySeedTrackA[trainIdx];
#else
    return trainFastVelocitySeedTrackB[trainIdx];
#endif
}

int getStoppingVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainStopVelocitySeedTrackA[trainIdx];
#else
    return trainStopVelocitySeedTrackB[trainIdx];
#endif
}

int getStoppingDistSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainStoppingDistSeedTrackA[trainIdx];
#else
    return trainStoppingDistSeedTrackB[trainIdx];
#endif
}

void initializeTrains(Train* trains, int marklinServerTid) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        trains[i].speed = 0;
        trains[i].id = trainAddresses[i];
        trains[i].reversing = false;
        trains[i].active = false;
        trains[i].velocity = 0;
        trains[i].nodeAhead = nullptr;
        trains[i].nodeBehind = nullptr;
        trains[i].sensorAhead = nullptr;
        trains[i].sensorBehind = nullptr;
        trains[i].stoppingSensor = nullptr;
        trains[i].whereToIssueStop = 0;
        trains[i].stoppingDistance = getStoppingDistSeed(i);
        trains[i].stopping = false;
        trains[i].targetNode = nullptr;
        trains[i].sensorWhereSpeedChangeStarted = nullptr;
        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainAddresses[i]);
    }
}

#include <algorithm>
#include <utility>

#include "clock_server_protocol.h"
#include "config.h"
#include "generic_protocol.h"
#include "localization_server_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "sys_call_stubs.h"
#include "timer.h"
#include "train_protocol.h"
#include "util.h"

#define TASK_MSG_SIZE 20
#define NOTIFIER_PRIORITY 15
#define STOPPING_THRESHOLD 50
// sensor bar to back, in mm
// this will need to be a variable and changed when we do reverses
#define DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN 200

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

using namespace train_server;

void TrainTask() {
    int parentTid = sys::MyParentTid();
    uint32_t clientTid = 0;

    unsigned int myTrainNumber = uIntReceive(&clientTid);
    emptyReply(clientTid);

    int trainIndex = trainNumToIndex(myTrainNumber);
    char* trainColour = nullptr;
    switch (myTrainNumber) {
        case 13:
            trainColour = "\033[38;5;33m[Train 13]:\033[m";
            break;
        case 14:
            trainColour = "\033[38;5;99m[Train 14]:\033[m";
            break;
        case 15:
            trainColour = "\033[38;5;163m[Train 15]:\033[m";
            break;
        case 17:
            trainColour = "\033[38;5;51m[Train 17]:\033[m";
            break;
        case 18:
            trainColour = "\033[38;5;154m[Train 18]:\033[m";
            break;
        case 55:
            trainColour = "\033[38;5;226m[Train 55]:\033[m";
            break;
    }

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

    ///////////////////////// VARIABLES /////////////////////////////////

    // ************ STATIC TRAIN STATE *************
    uint8_t speed = 0;              // Marklin Box Unit
    uint64_t stoppingDistance = 0;  // mm

    uint64_t slowingDown = 0;
    bool stopped = true;
    bool reversing = false;

    // ********** REAL-TIME TRAIN STATE ************
    uint64_t velocityEstimate = 0;  // in micrometers per microsecond (µm/µs) with a *1000 for decimals
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;

    Sensor sensorAhead;                       // sensor ahead of train
    Sensor sensorBehind;                      // sensor behind train
    uint64_t distanceToSensorAhead = 0;       // mm, static
    uint64_t distRemainingToSensorAhead = 0;  // mm, dynamic

    // ********** SENSOR MANAGEMENT ****************

    uint64_t prevSensorHitMicros = 0;
    uint64_t prevDistance = 0;
    int64_t prevSensorPredicitionMicros = 0;
    int64_t sensorAheadMicros = 0;
    uint64_t newSensorsPassed = 0;

    // *********** UPDATE MANAGEMENT ***************
    uint64_t prevNotificationMicros = 0;

    // ********* RESERVATION MANAGEMENT ************
    RingBuffer<ReservedZone, 16> reservedZones;
    RingBuffer<ZoneExit, 16> zoneExits;

    bool recentZoneAddedFlag = false;

    Sensor zoneEntraceSensorAhead;                        // sensor ahead of train marking a zone entrace
    uint64_t distanceToZoneEntraceSensorAhead = 0;        // mm, static
    uint64_t distRemainingToZoneEntranceSensorAhead = 0;  // mm, dynamic

    /////////////////////////////////////////////////////////////////////

    uint32_t notifierTid = sys::Create(NOTIFIER_PRIORITY, TrainNotifier);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == notifierTid, "train startup received from someone other than the train notifier");

    for (;;) {
        char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        sys::Receive(&clientTid, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

        if (clientTid == parentTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    emptyReply(clientTid);  // reply right away to reduce latency

                    unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);
                    if (!reversing && !slowingDown) {
                        if (trainSpeed == TRAIN_SPEED_8) {
                            marklin_server::setTrainSpeed(marklinServerTid, TRAIN_SPEED_9, myTrainNumber);
                        }
                        marklin_server::setTrainSpeed(marklinServerTid, trainSpeed, myTrainNumber);
                        if (stopped) stopped = false;
                    }

                    speed = trainSpeed;
                    velocityEstimate = getFastVelocitySeed(trainIndex);

                    if (trainSpeed == TRAIN_SPEED_8) {
                        velocityEstimate = getStoppingVelocitySeed(trainIndex);
                        stoppingDistance = getStoppingDistSeed(trainIndex);  // will need to be updated w acceleration
                    } else if (trainSpeed == TRAIN_STOP) {
                        slowingDown = timerGet();
                        // plug in acceleration model here lmao
                    }

                    while (!velocitySamples.empty()) {
                        velocitySamples.pop();
                    }
                    newSensorsPassed = 0;

                    break;
                }
                case Command::NEW_SENSOR: {
                    int64_t curMicros = timerGet();
                    emptyReply(clientTid);  // reply right away to reduce latency
                    if (newSensorsPassed > 1 && sensorAhead.box != 'F') {
                        ASSERT(receiveBuff[1] == sensorAhead.box &&
                                   static_cast<uint8_t>(receiveBuff[2]) == sensorAhead.num,
                               "Train %u was not expecting this sensor to come next");
                    }
                    Sensor curSensor{.box = receiveBuff[1], .num = static_cast<uint8_t>(receiveBuff[2])};

                    sensorAhead = Sensor{.box = receiveBuff[3], .num = static_cast<uint8_t>(receiveBuff[4])};

                    ASSERT(sensorAhead.num <= 16 && sensorAhead.num >= 1, "got some bad sensor num: %u",
                           static_cast<uint8_t>(receiveBuff[4]));
                    ASSERT(curSensor.num <= 16 && curSensor.num >= 1, "got some bad sensor num: %u",
                           static_cast<uint8_t>(receiveBuff[2]));

                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);
                    distanceToSensorAhead = distance;  // might actually be more if we pass a fake sensor
                    distanceToZoneEntraceSensorAhead = distance;

                    if (!prevSensorHitMicros) {
                        // ***************************  FIRST SENSOR HIT  ***************************
                        prevSensorHitMicros = curMicros;

                        ASSERT(velocityEstimate != 0, "We should have a seed initialized by now");

                        printer_proprietor::updateTrainStatus(printerProprietorTid, trainIndex, true);  // train active

                        // reserve current zone on initial sensor hit
                        char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                        localization_server::makeReservation(parentTid, trainIndex, curSensor, replyBuff);

                        switch (replyFromByte(replyBuff[0])) {
                            case Reply::RESERVATION_SUCCESS: {
                                printer_proprietor::debugPrintF(
                                    printerProprietorTid, "%s Initial Reservation with zone %u with sensor %c%u",
                                    trainColour, replyBuff[1], curSensor.box, curSensor.num);

                                zoneEntraceSensorAhead =
                                    Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
                                printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex,
                                                                          zoneEntraceSensorAhead);

                                unsigned int distance = 0;
                                a2ui(&replyBuff[4], 10, &distance);

                                ReservedZone reservation{.sensorMarkingEntrance = curSensor,
                                                         .zoneNum = static_cast<uint8_t>(replyBuff[1])};
                                reservedZones.push(reservation);
                                printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
                                break;
                            }
                            default: {
                                printer_proprietor::debugPrintF(printerProprietorTid,
                                                                "%s Initial Reservation FAILED with sensor %c%u",
                                                                trainColour, curSensor.box, curSensor.num);
                                break;
                            }
                        }

                        sensorBehind = curSensor;
                        emptyReply(notifierTid);  // ready for real-time train updates now
                        continue;
                    }

                    // *************************** AFTER FIRST SENSOR HIT  ***************************
                    newSensorsPassed++;
                    uint64_t microsDeltaT = curMicros - prevSensorHitMicros;
                    uint64_t mmDeltaD = prevDistance;
                    prevDistance = distance;

                    if (newSensorsPassed >= 5) {
                        uint64_t nextSample =
                            (mmDeltaD * 1000000) * 1000 / microsDeltaT;  // microm/micros with a *1000 for decimals
                        uint64_t lastEstimate = velocityEstimate;
                        velocityEstimate = ((lastEstimate * 15) + nextSample) >> 4;  // alpha = 1/8
                        newSensorsPassed = 5;
                    }

                    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);

                    prevSensorPredicitionMicros = sensorAheadMicros;
                    sensorAheadMicros =
                        curMicros + (distance * 1000 * 1000000 / velocityEstimate);  // when will hit sensorAhead

                    int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
                    int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
                    int64_t estimateDistanceDiffmm = ((int64_t)velocityEstimate * estimateTimeDiffmicros) / 1000000000;
                    printer_proprietor::updateSensor(printerProprietorTid, curSensor, estimateTimeDiffms,
                                                     estimateDistanceDiffmm);
                    printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, sensorAhead);

                    sensorBehind = curSensor;
                    prevSensorHitMicros = curMicros;

                    break;
                }
                default:
                    return;
            }

        } else if (clientTid == notifierTid) {  // so we have some velocity estimate to start with
            int64_t curMicros = timerGet();

            uint64_t timeSinceLastSensorHit = curMicros - prevSensorHitMicros;  // micros
            uint64_t timeSinceLastNoti = 0;                                     // micros

            if (!prevNotificationMicros) {  // first notification
                timeSinceLastNoti = curMicros - prevSensorHitMicros;
            } else {
                timeSinceLastNoti = curMicros - prevNotificationMicros;
            }

            uint64_t distTravelledSinceLastSensorHit =
                ((int64_t)velocityEstimate * timeSinceLastSensorHit) / 1000000000;  // mm

            distRemainingToSensorAhead = ((distanceToSensorAhead > distTravelledSinceLastSensorHit)
                                              ? (distanceToSensorAhead - distTravelledSinceLastSensorHit)
                                              : 0);

            distRemainingToZoneEntranceSensorAhead =
                ((distanceToZoneEntraceSensorAhead > distTravelledSinceLastSensorHit)
                     ? (distanceToZoneEntraceSensorAhead - distTravelledSinceLastSensorHit)
                     : 0);

            // if our stop will end up in the next zone, reserve it
            // fix this wack ass logic with flags
            printer_proprietor::updateTrainZoneDistance(printerProprietorTid, trainIndex, distRemainingToSensorAhead);
            if (distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !slowingDown &&
                !stopped) {
                // TODO: should this spin up a high priority courier? Or reply to one we already have?
                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                // printer_proprietor::debugPrintF(printerProprietorTid,
                //                                 "[Train %u] Attempting reservation with Zone Entrance Sensor: %c%d",
                //                                 myTrainNumber, zoneEntraceSensorAhead.box,
                //                                 zoneEntraceSensorAhead.num);
                localization_server::makeReservation(parentTid, trainIndex, zoneEntraceSensorAhead, replyBuff);

                switch (replyFromByte(replyBuff[0])) {
                    case Reply::RESERVATION_SUCCESS: {
                        uint64_t distToExitSensor =
                            distRemainingToZoneEntranceSensorAhead + DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN;

                        ReservedZone reservation{.sensorMarkingEntrance = zoneEntraceSensorAhead,
                                                 .zoneNum = static_cast<uint8_t>(replyBuff[1])};
                        reservedZones.push(reservation);

                        // let's create an exit entry for a previous zone
                        ZoneExit zoneExit{.sensorMarkingExit = zoneEntraceSensorAhead,
                                          .distanceToExitSensor = distToExitSensor};
                        zoneExits.push(zoneExit);

                        printer_proprietor::debugPrintF(
                            printerProprietorTid, "%s Made Reservation. Zone: %d, Zone Entrance Sensor: %c%d",
                            trainColour, replyBuff[1], zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num);

                        // we have a new zone entrace sensor we care about
                        zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
                        printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex,
                                                                  zoneEntraceSensorAhead);

                        unsigned int distance = 0;  // TODO: we are casting to unsigned int, when we have a uint64_t
                        a2ui(&replyBuff[4], 10, &distance);

                        //  dist to prevZoneEntranceSensorAhead + dist between prevZoneEntranceSensorAhead &
                        // new zoneEntranceSensorAhead
                        distanceToZoneEntraceSensorAhead = distanceToZoneEntraceSensorAhead + distance;
                        recentZoneAddedFlag = true;
                        printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);

                        break;
                    }
                    case Reply::RESERVATION_FAILURE: {
                        // need to break NOW
                        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                        slowingDown = curMicros;

                        printer_proprietor::debugPrintF(
                            printerProprietorTid, "%s FAILED Reservation for zone %d with sensor: %c%d", trainColour,
                            replyBuff[1], zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num);

                        // try again in a bit? after we stopped?
                        break;
                    }
                    default: {
                        printer_proprietor::debugPrintF(printerProprietorTid, "INVALID REPLY FROM makeReservation %d",
                                                        replyFromByte(replyBuff[0]));
                        break;
                    }
                }
            }

            if (distanceToSensorAhead > 0 && !slowingDown && !stopped) {
                printer_proprietor::updateTrainDistance(printerProprietorTid, trainIndex, distRemainingToSensorAhead);
            } else if (((curMicros - slowingDown) / 1000) > 1000 * 6) {  // 6 seconds for slowing down?
                // insert acceleration model here
                slowingDown = 0;
                stopped = true;
                prevNotificationMicros = curMicros;
                // emptyReply(clientTid);
                continue;
            } else if (stopped) {
                // try and make a reservation request again. If it works, go back to your speed?
                prevNotificationMicros = curMicros;
                continue;
            } else {
                prevNotificationMicros = curMicros;
                continue;
            }

            // for (auto it = zoneExits.begin(); it != zoneExits.end(); ++it) {  // update distance to zone exit sensors
            //     Sensor zoneExitSensor = (*it).sensorMarkingExit;

            //     if (zoneExitSensor == zoneExits.back()->sensorMarkingExit && recentZoneAddedFlag) {
            //         // this should only potentially happen on the last one, if we just added a zone and don't want to
            //         //  subtract our travelled distance to it on the same iteration we added it (wait until next
            //         notif) recentZoneAddedFlag = false; continue;
            //     }

            //     uint64_t distanceTravelledSinceLastNoti = (velocityEstimate * timeSinceLastNoti) / 1000000000;  // mm

            //     uint64_t distanceToZoneExitSensor = (*it).distanceToExitSensor;

            //     (*it).distanceToExitSensor = (distanceToZoneExitSensor > distanceTravelledSinceLastNoti)
            //                                      ? (distanceToZoneExitSensor - distanceTravelledSinceLastNoti)
            //                                      : 0;
            // }

            if (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit == sensorBehind &&
                distTravelledSinceLastSensorHit >=
                    DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN) {  // can we free any zones?
                Sensor reservationSensor = zoneExits.front()->sensorMarkingExit;

                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                localization_server::freeReservation(parentTid, trainIndex, reservationSensor, replyBuff);

                switch (replyFromByte(replyBuff[0])) {
                    case Reply::FREE_SUCCESS: {
                        printer_proprietor::debugPrintF(
                            printerProprietorTid, "%s Freed Reservation with zone: %d with sensor: %c%d", trainColour,
                            replyBuff[1], reservationSensor.box, reservationSensor.num);

                        zoneExits.pop();
                        reservedZones.pop();
                        printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
                        break;
                    }
                    default: {
                        printer_proprietor::debugPrintF(printerProprietorTid,
                                                        "%s Freed Reservation FAILED with sensor %c%d", trainColour,
                                                        reservationSensor.box, reservationSensor.num);
                        break;
                    }
                }
            }

            prevNotificationMicros = curMicros;
            emptyReply(clientTid);
        } else {
            ASSERT(0, "someone else sent to train task?");
        }
    }
}
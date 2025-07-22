#include "train.h"

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
#include "train_protocol.h"
#include "util.h"

static const int trainAddresses[MAX_TRAINS] = {13, 14, 15, 17, 18, 55};
// train 55 is 10H

// velocities are mm/s * 1000 for decimals
static const uint64_t trainFastVelocitySeedTrackB[MAX_TRAINS] = {596041, 605833, 596914, 266566, 627366, 525104};

// acceleration are mm/s^2 * 1000 for decimals
static const uint64_t trainAccelSeedTrackB[MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackB[MAX_TRAINS] = {91922, 91922, 91922, 91922, 91922, 91922};
static const uint64_t trainStopVelocitySeedTrackB[MAX_TRAINS] = {253549, 257347, 253548, 266566, 266566, 311583};
static const uint64_t trainStoppingDistSeedTrackB[MAX_TRAINS] = {400, 400, 400, 400, 400, 400};

// speed 6, loosely for tr 14
static const uint64_t trainSlowVelocitySeedTrackB[MAX_TRAINS] = {132200, 132200, 132200, 132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackB[MAX_TRAINS] = {165, 165, 165, 165, 165, 165};
static const uint64_t trainSlowVelocitySeedTrackA[MAX_TRAINS] = {132200, 132200, 132200, 132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackA[MAX_TRAINS] = {165, 165, 165, 165, 165, 165};

static const uint64_t trainFastVelocitySeedTrackA[MAX_TRAINS] = {601640, 605833, 596914, 266566, 625911, 525104};
static const uint64_t trainAccelSeedTrackA[MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackA[MAX_TRAINS] = {91922, 91922, 91922, 91922, 91922, 91922};
static const uint64_t trainStopVelocitySeedTrackA[MAX_TRAINS] = {254904, 257347, 253548, 266566, 265294, 311583};
static const uint64_t trainStoppingDistSeedTrackA[MAX_TRAINS] = {400, 400, 400, 400, 410, 400};

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}

int trainIndexToNum(int trainIndex) {
    return trainAddresses[trainIndex];
}

uint64_t getFastVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainFastVelocitySeedTrackA[trainIdx];
#else
    return trainFastVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getStoppingVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainStopVelocitySeedTrackA[trainIdx];
#else
    return trainStopVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getAccelerationSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainAccelSeedTrackB[trainIdx];
#else
    return trainAccelSeedTrackB[trainIdx];
#endif
}

uint64_t getDecelerationSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainDecelSeedTrackA[trainIdx];
#else
    return trainDecelSeedTrackB[trainIdx];
#endif
}

uint64_t getSlowVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainSlowVelocitySeedTrackA[trainIdx];
#else
    return trainSlowVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getSlowStoppingDistSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < MAX_TRAINS);

#if defined(TRACKA)
    return trainSlowStoppingDistSeedTrackA[trainIdx];
#else
    return trainSlowStoppingDistSeedTrackB[trainIdx];
#endif
}

uint64_t getStoppingDistSeed(int trainIdx) {
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
        trains[i].isReversing = false;
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

int stopDistFromSpeed(int trainIndex, int speed) {
    switch (speed) {
        case TRAIN_SPEED_14:
            return getStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_8:
            return getStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_6:
            return getSlowStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        default:
            return TRAIN_STOP;
    }
}

// insert acceleration model here...
uint64_t velocityFromSpeed(int trainIndex, int speed) {
    switch (speed) {
        case TRAIN_SPEED_14:
            return getFastVelocitySeed(trainIndex);
        case TRAIN_SPEED_8:
            return getStoppingVelocitySeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_6:
            return getSlowVelocitySeed(trainIndex);  // speed 14 does not have a stopping dist
        default:
            return TRAIN_STOP;
    }
}

#define TASK_MSG_SIZE 20
#define NOTIFIER_PRIORITY 15
#define STOPPING_THRESHOLD 10     // 1 cm
#define TRAIN_SENSOR_TIMEOUT 300  // 5 sec?
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

    uint32_t reverseTid = 0;

    TrackNode track[TRACK_MAX];

#if defined(TRACKA)
    init_tracka(track);
#else

    init_trackb(track);
#endif
    uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX];
    initializeDistanceMatrix(track, distanceMatrix);

    ///////////////////////// VARIABLES /////////////////////////////////

    // ************ STATIC TRAIN STATE *************
    uint8_t speed = 0;              // Marklin Box Unit
    uint64_t stoppingDistance = 0;  // mm

    uint64_t slowingDown = 0;
    bool stopped = true;
    bool isForward = true;

    // ********** REAL-TIME TRAIN STATE ************
    uint64_t velocityEstimate = 0;  // in micrometers per microsecond (µm/µs) with a *1000 for decimals
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;

    Sensor sensorAhead;                       // sensor ahead of train
    Sensor sensorBehind;                      // sensor behind train
    Sensor stopSensor;                        // sensor we are waiting to hit
    uint64_t stopSensorOffset = 0;            // mm, static
    uint64_t distanceToSensorAhead = 0;       // mm, static
    uint64_t distRemainingToSensorAhead = 0;  // mm, dynamic

    bool isAccelerating = false;
    uint64_t accelerationStartTime = 0;  // micros
    uint64_t totalAccelerationTime = 0;  // micros

    bool isReversing = false;
    uint64_t reversingStartTime = 0;  // micros
    uint64_t totalReversingTime = 0;  // micros

    // ********** SENSOR MANAGEMENT ****************

    uint64_t prevSensorHitMicros = 0;
    uint64_t prevDistance = 0;
    int64_t prevSensorPredicitionMicros = 0;
    int64_t sensorAheadMicros = 0;
    uint64_t newSensorsPassed = 0;

    // *********** UPDATE MANAGEMENT ***************
    uint64_t prevNotificationMicros = 0;

    // ********* RESERVATION MANAGEMENT ************
    RingBuffer<ReservedZone, 32> reservedZones;
    RingBuffer<ZoneExit, 32> zoneExits;

    bool recentZoneAddedFlag = false;

    Sensor zoneEntraceSensorAhead;                        // sensor ahead of train marking a zone entrace
    uint64_t distanceToZoneEntraceSensorAhead = 0;        // mm, static
    uint64_t distRemainingToZoneEntranceSensorAhead = 0;  // mm, dynamic

    /////////////////////////////////////////////////////////////////////

    uint32_t notifierTid = sys::Create(NOTIFIER_PRIORITY, TrainNotifier);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == notifierTid, "train startup received from someone other than the train notifier");

    uint32_t stopNotifierTid = sys::Create(42, StopTrain);  // train updater
    emptyReceive(&clientTid);
    ASSERT(clientTid == stopNotifierTid, "train startup received from someone other than the stop train notifier");

    for (;;) {
        char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        sys::Receive(&clientTid, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);

        if (clientTid == parentTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    emptyReply(clientTid);  // reply right away to reduce latency

                    unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);
                    if (!isReversing && !slowingDown) {
                        printer_proprietor::debugPrintF(printerProprietorTid, "%s Setting speed to %u ", trainColour,
                                                        trainSpeed);
                        marklin_server::setTrainSpeed(marklinServerTid, trainSpeed, myTrainNumber);
                    } else {
                        printer_proprietor::debugPrintF(printerProprietorTid,
                                                        "%s FAILED to set speed to %u. reversing: %u slowingDown: %u ",
                                                        trainColour, trainSpeed, isReversing, slowingDown);
                    }

                    speed = trainSpeed;

                    isAccelerating = true;
                    accelerationStartTime = timerGet();

                    totalAccelerationTime =
                        (velocityFromSpeed(trainIndex, speed) * 1000000) / getAccelerationSeed(trainIndex);  // micros

                    // velocityEstimate = velocityFromSpeed(trainIndex, trainSpeed);
                    stoppingDistance = stopDistFromSpeed(trainIndex, trainSpeed);

                    if (trainSpeed == TRAIN_STOP) {
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

                    // check if it's a stop sensor we're expecting so we can spin up notifier
                    if (curSensor.box == stopSensor.box && curSensor.num == stopSensor.num) {
                        uint64_t arrivalTime = (stopSensorOffset * 1000 * 1000000 / velocityEstimate);
                        uint16_t numOfTicks = (arrivalTime) / Config::TICK_SIZE;
                        ASSERT(stopNotifierTid < 100);
                        uIntReply(stopNotifierTid, numOfTicks);
                        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                        slowingDown = curMicros;
                    }

                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);
                    distanceToSensorAhead = distance;  // might actually be more if we pass a fake sensor

                    int curSensorIdx = trackNodeIdxFromSensor(curSensor);

                    if (!prevSensorHitMicros) {
                        // ***************************  FIRST SENSOR HIT  ***************************
                        prevSensorHitMicros = curMicros;

                        printer_proprietor::updateTrainStatus(printerProprietorTid, trainIndex, true);  // train active
                        stopped = false;

                        // reserve current zone on initial sensor hit
                        char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                        localization_server::makeReservation(parentTid, trainIndex, curSensor, replyBuff);

                        switch (replyFromByte(replyBuff[0])) {
                            case Reply::RESERVATION_SUCCESS: {
                                // printer_proprietor::debugPrintF(
                                //     printerProprietorTid, "%s Initial Reservation with zone %u with sensor %c%u",
                                //     trainColour, replyBuff[1], curSensor.box, curSensor.num);

                                zoneEntraceSensorAhead =
                                    Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
                                printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex,
                                                                          zoneEntraceSensorAhead);

                                int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);

                                // unsigned int distance = 0;
                                // a2ui(&replyBuff[4], 10, &distance);

                                distanceToZoneEntraceSensorAhead =
                                    distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];

                                ReservedZone reservation{.sensorMarkingEntrance = curSensor,
                                                         .zoneNum = static_cast<uint8_t>(replyBuff[1])};
                                reservedZones.push(reservation);
                                printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
                                break;
                            }
                            default: {
                                printer_proprietor::debugPrintF(
                                    printerProprietorTid,
                                    "%s \033[5m \033[38;5;160m Initial Reservation FAILED with sensor %c%u \033[m",
                                    trainColour, curSensor.box, curSensor.num);
                                break;
                            }
                        }

                        sensorBehind = curSensor;
                        emptyReply(notifierTid);  // ready for real-time train updates now
                        continue;
                    } else if (stopped) {
                        // && !(stopSensor == sensorAhead)
                        prevSensorHitMicros = curMicros;
                        // if we were stopped, but now we hit a sensor. We clearly are no longer stopped
                        stopped = false;
                        sensorBehind = curSensor;
                        emptyReply(notifierTid);  // ready for real-time train updates now
                        continue;
                    }

                    // *************************** AFTER FIRST SENSOR HIT  ***************************

                    int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);
                    distanceToZoneEntraceSensorAhead = distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];

                    newSensorsPassed++;
                    uint64_t microsDeltaT = curMicros - prevSensorHitMicros;
                    uint64_t mmDeltaD = prevDistance;
                    prevDistance = distance;

                    if (!isAccelerating) {
                        uint64_t nextSample =
                            (mmDeltaD * 1000000) * 1000 / microsDeltaT;  // microm/micros with a *1000 for decimals
                        uint64_t lastEstimate = velocityEstimate;
                        velocityEstimate = ((lastEstimate * 15) + nextSample) >> 4;  // alpha = 1/8
                        newSensorsPassed = 5;
                    }

                    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);

                    prevSensorPredicitionMicros = sensorAheadMicros;
                    sensorAheadMicros = curMicros + ((distanceToSensorAhead * 1000 * 1000000) /
                                                     velocityEstimate);  // when will hit sensorAhead
                    // printer_proprietor::debugPrintF(
                    //     printerProprietorTid,
                    //     "%s distance: %u [denominator] : velocity estimate : %u  fraction result: %u", trainColour,
                    //     distanceToSensorAhead, velocityEstimate,
                    //     (distanceToSensorAhead * 1000 * 1000000) / velocityEstimate);

                    int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
                    int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
                    int64_t estimateDistanceDiffmm = ((int64_t)velocityEstimate * estimateTimeDiffmicros) / 1000000000;

                    // this is clearly broken atm
                    printer_proprietor::updateSensor(printerProprietorTid, curSensor, estimateTimeDiffms,
                                                     estimateDistanceDiffmm);

                    printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, sensorAhead);

                    sensorBehind = curSensor;
                    // printer_proprietor::debugPrintF(
                    //     printerProprietorTid,
                    //     "%s Sensor Read: %c%d Current time of sensor hit: %u ms Last time: %u ms Difference: %u ms",
                    //     trainColour, curSensor.box, curSensor.num, curMicros / 1000, prevSensorHitMicros / 1000,
                    //     (curMicros - prevSensorHitMicros) / 1000);
                    prevSensorHitMicros = curMicros;

                    // if you had an old zone that should have been freed
                    if (!zoneExits.empty() && !(zoneExits.front()->sensorMarkingExit == curSensor)) {
                        // check
                        while (!zoneExits.empty() && !(zoneExits.front()->sensorMarkingExit == curSensor)) {
                            Sensor reservationSensor = zoneExits.front()->sensorMarkingExit;
                            char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                            localization_server::freeReservation(parentTid, trainIndex, reservationSensor, replyBuff);

                            switch (replyFromByte(replyBuff[0])) {
                                case Reply::FREE_SUCCESS: {
                                    printer_proprietor::debugPrintF(printerProprietorTid,
                                                                    "%s \033[48;5;17m (Backup) Freed Reservation "
                                                                    "with zone: %d with sensor: %c%d",
                                                                    trainColour, replyBuff[1], reservationSensor.box,
                                                                    reservationSensor.num);

                                    zoneExits.pop();
                                    reservedZones.pop();
                                    printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex,
                                                                        reservedZones);
                                    break;
                                }
                                default: {
                                    printer_proprietor::debugPrintF(
                                        printerProprietorTid, "%s (Backup) Freed Reservation FAILED with sensor %c%d",
                                        trainColour, reservationSensor.box, reservationSensor.num);
                                    break;
                                }
                            }
                        }
                    }

                    break;
                }
                case Command::REVERSE: {
                    emptyReply(clientTid);  // reply right away to reduce latency
                    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                    reverseTid = sys::Create(40, &marklin_server::reverseTrainTask);
                    isReversing = true;
                    reversingStartTime = timerGet();
                    totalReversingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
                    printer_proprietor::debugPrintF(printerProprietorTid, "TOTAL REVERSE TIME %u", totalReversingTime);
                    break;
                }

                case Command::STOP_SENSOR: {
                    int64_t curMicros = timerGet();
                    emptyReply(clientTid);  // reply right away to reduce latency
                    stopSensor.box = receiveBuff[1];
                    stopSensor.num = static_cast<uint8_t>(receiveBuff[2]);
                    unsigned int distance = 0;
                    a2ui(&receiveBuff[5], 10, &distance);
                    stopSensorOffset = distance;
                    break;
                }

                default:
                    return;
            }
        } else if (clientTid == notifierTid) {  // so we have some velocity estimate to start with
            int64_t curMicros = timerGet();

            if (isAccelerating) {
                uint64_t timeSinceAccelerationStart = curMicros - accelerationStartTime;
                if (timeSinceAccelerationStart >= totalAccelerationTime) {
                    velocityEstimate = velocityFromSpeed(trainIndex, speed);
                    isAccelerating = false;
                } else {
                    velocityEstimate = (getAccelerationSeed(trainIndex) * timeSinceAccelerationStart) / 1000000;
                }
                printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
            }

            uint64_t timeSinceLastSensorHit = curMicros - prevSensorHitMicros;  // micros
            uint64_t timeSinceLastNoti = 0;                                     // micros

            // have some timeout here
            // if ((curMicros - prevSensorPredicitionMicros) / 1000 > TRAIN_SENSOR_TIMEOUT && newSensorsPassed >= 5) {
            //     // do some kind of reset thing
            //     stopped = true;
            //     continue;
            // }

            if (!prevNotificationMicros) {  // skip first notification so we have a delta T
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
            if (!stopped) {
                printer_proprietor::updateTrainZoneDistance(printerProprietorTid, trainIndex,
                                                            distRemainingToZoneEntranceSensorAhead - stoppingDistance);
            }

            if (distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !stopped &&
                !(sensorAhead == stopSensor)) {
                ASSERT(stoppingDistance > 0);
                // TODO: should this spin up a high priority courier? Or reply to one we already have?
                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                // printer_proprietor::debugPrintF(printerProprietorTid,
                //                                 "[Train %u] Attempting reservation with Zone Entrance Sensor:
                //                                 %c%d", myTrainNumber, zoneEntraceSensorAhead.box,
                //                                 zoneEntraceSensorAhead.num);
                localization_server::makeReservation(parentTid, trainIndex, zoneEntraceSensorAhead, replyBuff);

                switch (replyFromByte(replyBuff[0])) {
                    case Reply::RESERVATION_SUCCESS: {
                        uint64_t distToExitSensor =
                            distRemainingToZoneEntranceSensorAhead + DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN;
                        // ASSERT(distToExitSensor > DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN,
                        //        "distRemainingToZoneEntranceSensorAhead was somehow zero when we should be
                        //        reserving " "much sooner");
                        ReservedZone reservation{.sensorMarkingEntrance = zoneEntraceSensorAhead,
                                                 .zoneNum = static_cast<uint8_t>(replyBuff[1])};
                        reservedZones.push(reservation);

                        // let's create an exit entry for a previous zone
                        ZoneExit zoneExit{.sensorMarkingExit = zoneEntraceSensorAhead,
                                          .distanceToExitSensor = distToExitSensor};
                        zoneExits.push(zoneExit);

                        // we have a new zone entrace sensor we care about
                        zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
                        printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex,
                                                                  zoneEntraceSensorAhead);

                        unsigned int distance = 0;  // TODO: we are casting to unsigned int, when we have a uint64_t
                        a2ui(&replyBuff[4], 10, &distance);

                        printer_proprietor::debugPrintF(
                            printerProprietorTid,
                            "%s \033[48;5;22m Made Reservation for Zone: %d using Zone Entrance "
                            "Sensor: %c%d because I was %u mm away. Next zone "
                            "entrance sensor is %c%d which is %u mm away",
                            trainColour, replyBuff[1], zoneExit.sensorMarkingExit.box, zoneExit.sensorMarkingExit.num,
                            distRemainingToZoneEntranceSensorAhead, zoneEntraceSensorAhead.box,
                            zoneEntraceSensorAhead.num, distanceToZoneEntraceSensorAhead + distance);

                        //  dist to prevZoneEntranceSensorAhead + dist between prevZoneEntranceSensorAhead &
                        // new zoneEntranceSensorAhead

                        int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);
                        int sensorAheadIdx = trackNodeIdxFromSensor(sensorAhead);
                        distanceToZoneEntraceSensorAhead =
                            distanceMatrix[sensorAheadIdx][zoneEntraceSensorAheadIdx] + distanceToSensorAhead;

                        recentZoneAddedFlag = true;
                        printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);

                        if (slowingDown) {
                            slowingDown = 0;
                            marklin_server::setTrainSpeed(marklinServerTid, speed, myTrainNumber);
                        }

                        break;
                    }
                    case Reply::RESERVATION_FAILURE: {
                        // need to break NOW
                        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                        slowingDown = curMicros;

                        printer_proprietor::debugPrintF(
                            printerProprietorTid,
                            "%s \033[5m \033[38;5;160m FAILED Reservation for zone %d with sensor: %c%d \033[m",
                            trainColour, replyBuff[1], zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num);

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
            } else if (((curMicros - slowingDown) / (1000 * 1000)) > 5 && slowingDown) {  // 6 seconds for slowing down?
                // insert decceleration model here
                slowingDown = 0;
                stopped = true;
                prevSensorHitMicros = 0;
                prevNotificationMicros = curMicros;
                localization_server::updateReservation(parentTid, trainIndex, reservedZones, ReservationType::STOPPED);
                printer_proprietor::debugPrintF(printerProprietorTid, "Assume we have updated our reservation types");
                // I want to uncomment this but I think it breaks the console w some infinite loop of failing a
                // reservation
                // emptyReply(clientTid);
                // continue;
            } else if (stopped && !(stopSensor == sensorAhead)) {
                // try and make a reservation request again. If it works, go back to your speed?
                ASSERT(!slowingDown, "we have stopped but 'stopping' has a timestamp");

                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
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

                        // we have a new zone entrance sensor we care about
                        zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
                        printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex,
                                                                  zoneEntraceSensorAhead);

                        unsigned int distance = 0;  // TODO: we are casting to unsigned int, when we have a uint64_t
                        a2ui(&replyBuff[4], 10, &distance);

                        printer_proprietor::debugPrintF(
                            printerProprietorTid,
                            "%s \033[48;5;22m (Wakeup) Made Reservation for Zone: %d using Zone Entrance "
                            "Sensor: %c%d ",
                            trainColour, replyBuff[1], zoneExit.sensorMarkingExit.box, zoneExit.sensorMarkingExit.num,
                            distRemainingToZoneEntranceSensorAhead, zoneEntraceSensorAhead.box);

                        int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);
                        int sensorAheadIdx = trackNodeIdxFromSensor(sensorAhead);
                        distanceToZoneEntraceSensorAhead =
                            distanceMatrix[sensorAheadIdx][zoneEntraceSensorAheadIdx] + distanceToSensorAhead;

                        recentZoneAddedFlag = true;
                        printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
                        marklin_server::setTrainSpeed(marklinServerTid, speed,
                                                      myTrainNumber);  // go back up to our speed
                        break;
                    }
                    case Reply::RESERVATION_FAILURE: {
                        // We are already stopped
                        clock_server::Delay(clockServerTid, 10);  // delay 10 ticks before replying to notifier
                        break;
                    }
                    default: {
                        printer_proprietor::debugPrintF(printerProprietorTid, "INVALID REPLY FROM makeReservation %d",
                                                        replyFromByte(replyBuff[0]));
                        break;
                    }
                }

            } else if (stopped) {
                // we are stopped, generate a new stop location
                prevNotificationMicros = curMicros;
                ASSERT(!slowingDown, "we have stopped but 'stopping' has a timestamp");

                continue;  // DO NOT REPLY TO NOTIFIER
            }
            // otherwise, we are slowing down

            if (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit == sensorBehind &&
                (distTravelledSinceLastSensorHit >=
                 DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN)) {  // can we free any zones?
                Sensor reservationSensor = zoneExits.front()->sensorMarkingExit;

                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                localization_server::freeReservation(parentTid, trainIndex, reservationSensor, replyBuff);

                switch (replyFromByte(replyBuff[0])) {
                    case Reply::FREE_SUCCESS: {
                        printer_proprietor::debugPrintF(
                            printerProprietorTid, "%s \033[48;5;17m Freed Reservation with zone: %d with sensor: %c%d",
                            trainColour, replyBuff[1], reservationSensor.box, reservationSensor.num);

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
            emptyReply(clientTid);  // reply to our notifier HERE
        } else if (clientTid == reverseTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::GET_REVERSE_TIME: {
                    uint64_t curTime = timerGet();
                    uint64_t timeToStop = (totalReversingTime - (curTime - reversingStartTime)) / 1000;  // millis
                    uint64_t ticksToStop = (timeToStop + 9) / 10;                                        // round up
                    printer_proprietor::debugPrintF(printerProprietorTid, "TOTAL TICKS TO STOP %u", ticksToStop);

                    uIntReply(clientTid, ticksToStop);
                    break;
                }
                case Command::FINISH_REVERSE: {
                    ASSERT(reverseTid != 0);
                    ASSERT(isReversing);
                    marklin_server::setTrainReverseAndSpeed(marklinServerTid, speed, myTrainNumber);
                    isReversing = false;
                    isForward != isForward;
                    // change our stopping distance and orientation
                    printer_proprietor::updateTrainOrientation(printerProprietorTid, trainIndex, isForward);
                    // should update our stopping distance
                    if (!isForward) {
                        // stopping distance is the distance from sensor bar to front of train + front of train to stop
                        // isReversing would mean we need to subtract this distance ^
                        // from the distance of the back of the train to the sensor bar
                        // back of train to sensor bar: 14.5 cm
                        stoppingDistance += 115;
                    } else {
                        stoppingDistance -= 115;
                    }

                    isAccelerating = true;
                    accelerationStartTime = timerGet();
                    totalAccelerationTime =
                        (velocityFromSpeed(trainIndex, speed) * 1000000) / getAccelerationSeed(trainIndex);  // micros

                    break;
                }
                default: {
                    printer_proprietor::debugPrintF(printerProprietorTid, "UNKOWN COMMAND");
                    break;
                }
            }
        } else if (clientTid == stopNotifierTid) {
            marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
            slowingDown = timerGet();
        } else {
            ASSERT(0, "someone else sent to train task?");
        }
    }
}
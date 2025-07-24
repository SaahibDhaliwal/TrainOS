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

using namespace train_server;
#define TASK_MSG_SIZE 20
#define NOTIFIER_PRIORITY 15
#define STOPPING_THRESHOLD 10     // 1 cm
#define TRAIN_SENSOR_TIMEOUT 300  // 5 sec?
// sensor bar to back, in mm
// this will need to be a variable and changed when we do reverses
#define DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN 200

Train::Train(unsigned int myTrainNumber, int printerProprietorTid, int marklinServerTid, int clockServerTid)
    : myTrainNumber(myTrainNumber),
      printerProprietorTid(printerProprietorTid),
      marklinServerTid(marklinServerTid),
      clockServerTid(clockServerTid),
      trainIndex(trainNumToIndex(myTrainNumber)) {
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
    }

#if defined(TRACKA)
    init_tracka(track);
#else

    init_trackb(track);
#endif
    uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX];
    initializeDistanceMatrix(track, distanceMatrix);
}

void Train::setTrainSpeed(unsigned int trainSpeed) {
    // unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);
    if (!isReversing && !slowingDown) {
        printer_proprietor::debugPrintF(printerProprietorTid, "%s Setting speed to %u ", trainColour, trainSpeed);
        marklin_server::setTrainSpeed(marklinServerTid, trainSpeed, myTrainNumber);
    } else {
        printer_proprietor::debugPrintF(printerProprietorTid,
                                        "%s FAILED to set speed to %u. reversing: %u slowingDown: %u ", trainColour,
                                        trainSpeed, isReversing, slowingDown);
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
        totalStoppingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
    }

    while (!velocitySamples.empty()) {
        velocitySamples.pop();
    }
    newSensorsPassed = 0;
}

void Train::initialSensorHit(int64_t curMicros, Sensor curSensor, int curSensorIdx) {
    // int64_t curMicros = timerGet();
    prevSensorHitMicros = curMicros;

    printer_proprietor::updateTrainStatus(printerProprietorTid, trainIndex, true);  // train active
    stopped = false;

    // reserve current zone on initial sensor hit
    char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    localization_server::initReservation(parentTid, trainIndex, curSensor, replyBuff);

    switch (replyFromByte(replyBuff[0])) {
        case Reply::RESERVATION_SUCCESS: {
            zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
            printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex, zoneEntraceSensorAhead);

            int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);

            distanceToZoneEntraceSensorAhead = distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];

            ReservedZone reservation{.sensorMarkingEntrance = curSensor, .zoneNum = static_cast<uint8_t>(replyBuff[1])};
            reservedZones.push(reservation);
            printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
            break;
        }
        default: {
            printer_proprietor::debugPrintF(
                printerProprietorTid, "%s \033[5m \033[38;5;160m Initial Reservation FAILED with sensor %c%u \033[m",
                trainColour, curSensor.box, curSensor.num);
            break;
        }
    }

    emptyReply(notifierTid);  // ready for real-time train updates now
}

void Train::newSensorHit(int64_t curMicros, Sensor curSensor, int curSensorIdx) {
    // if we came to a stop and chose to start moving again, we should attempt to reserve the next zone then
    if (stopped) {
        stopped = false;
        // anything else here?
    }

    int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);
    distanceToZoneEntraceSensorAhead = distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];

    newSensorsPassed++;
    uint64_t microsDeltaT = curMicros - prevSensorHitMicros;
    // im changing this.. should be fine V
    uint64_t mmDeltaD = prevDistance;
    prevDistance = distanceToSensorAhead;

    // only update velocity estimate if we aren't accelerating
    if (!isAccelerating) {
        uint64_t nextSample = (mmDeltaD * 1000000) * 1000 / microsDeltaT;  // microm/micros with a *1000 for decimals
        uint64_t lastEstimate = velocityEstimate;
        velocityEstimate = ((lastEstimate * 15) + nextSample) >> 4;  // alpha = 1/8
        newSensorsPassed = 5;
        printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
    }

    // when did we plan on hitting this sensor vs when did we actually hit it
    prevSensorPredicitionMicros = sensorAheadMicros;
    sensorAheadMicros =
        curMicros + ((distanceToSensorAhead * 1000 * 1000000) / velocityEstimate);  // when will hit sensorAhead

    int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
    int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
    int64_t estimateDistanceDiffmm = ((int64_t)velocityEstimate * estimateTimeDiffmicros) / 1000000000;

    printer_proprietor::updateSensor(printerProprietorTid, curSensor, estimateTimeDiffms, estimateDistanceDiffmm);
    printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, sensorAhead);

    prevSensorHitMicros = curMicros;
    tryToFreeZones();
}

void Train::processSensorHit(Sensor currentSensor, Sensor upcomingSensor, uint64_t distanceToSensorAhead) {
    int64_t curMicros = timerGet();
    if (newSensorsPassed > 1 && sensorAhead.box != 'F') {
        ASSERT(upcomingSensor.box == sensorAhead.box && upcomingSensor.num == sensorAhead.num,
               "Train %u was not expecting this sensor to come next");
    }

    // check if it's a stop sensor we're expecting so we can spin up notifier
    if (currentSensor.box == stopSensor.box && currentSensor.num == stopSensor.num) {
        uint64_t arrivalTime = (stopSensorOffset * 1000 * 1000000 / velocityEstimate);
        uint64_t numOfTicks = (arrivalTime) / Config::TICK_SIZE;
        ASSERT(stopNotifierTid < 100);
        uIntReply(stopNotifierTid, numOfTicks);
        isSlowingDown = true;
        stopStartTime = curMicros;
    }

    int curSensorIdx = trackNodeIdxFromSensor(currentSensor);
    // check if it's an initial sensor hit:
    if (!prevSensorHitMicros) {
        initialSensorHit(curMicros, currentSensor, curSensorIdx);
        // return;
    } else {
        newSensorHit(curMicros, currentSensor, curSensorIdx);
    }
    sensorBehind = currentSensor;

    // free things here
}

void Train::reverseCommand() {
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
    reverseTid = sys::Create(40, &marklin_server::reverseTrainTask);

    // should we change is reversing to one bool and have slowing
    isSlowingDown = true;
    isReversing = true;

    reversingStartTime = timerGet();
    totalReversingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
    printer_proprietor::debugPrintF(printerProprietorTid, "TOTAL REVERSE TIME %u", totalReversingTime);
}

void Train::newStopLocation(Sensor newStopSensor, Sensor newTargetSensor, uint64_t offset) {
    stopSensor = newStopSensor;
    targetSensor = newTargetSensor;
    printer_proprietor::updateTrainDestination(printerProprietorTid, trainIndex, targetSensor);
    stopSensorOffset = offset;
}

void Train::updateVelocitywithAcceleration(int64_t curMicros) {
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
}

bool Train::attemptReservation(int64_t curMicros) {
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
            ZoneExit zoneExit{.sensorMarkingExit = zoneEntraceSensorAhead, .distanceToExitSensor = distToExitSensor};
            zoneExits.push(zoneExit);

            // we have a new zone entrace sensor we care about
            zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
            printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex, zoneEntraceSensorAhead);

            unsigned int distance = 0;  // TODO: we are casting to unsigned int, when we have a uint64_t
            a2ui(&replyBuff[4], 10, &distance);

            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s \033[48;5;22m Made Reservation for Zone: %d using Zone Entrance "
                                            "Sensor: %c%d because I was %u mm away. Next zone "
                                            "entrance sensor is %c%d which is %u mm away",
                                            trainColour, replyBuff[1], zoneExit.sensorMarkingExit.box,
                                            zoneExit.sensorMarkingExit.num, distRemainingToZoneEntranceSensorAhead,
                                            zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num,
                                            distanceToZoneEntraceSensorAhead + distance);

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
            return true;
            break;
        }
        case Reply::RESERVATION_FAILURE: {
            // need to break NOW
            if (!slowingDown) {
                marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                slowingDown = curMicros;
                totalStoppingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);

                printer_proprietor::debugPrintF(
                    printerProprietorTid,
                    "%s \033[5m \033[38;5;160m FAILED Reservation for zone %d with sensor: %c%d \033[m", trainColour,
                    replyBuff[1], zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num);
            }

            // try again in a bit? after we stopped?
            return false;
            break;
        }
        default: {
            printer_proprietor::debugPrintF(printerProprietorTid, "INVALID REPLY FROM makeReservation %d",
                                            replyFromByte(replyBuff[0]));
            return false;
            break;
        }
    }
}

void Train::processQuickNotifier(uint64_t notifierTid) {
    int64_t curMicros = timerGet();

    uint64_t timeSinceLastSensorHit = curMicros - prevSensorHitMicros;  // micros
    uint64_t timeSinceLastNoti = 0;                                     // micros
    if (!prevNotificationMicros) {                                      // skip first notification so we have a delta T
        timeSinceLastNoti = curMicros - prevSensorHitMicros;
    } else {
        timeSinceLastNoti = curMicros - prevNotificationMicros;
    }

    // have some timeout here
    // if ((curMicros - prevSensorPredicitionMicros) / 1000 > TRAIN_SENSOR_TIMEOUT && newSensorsPassed >= 5) {
    //     // do some kind of reset thing
    //     stopped = true;
    //     continue;
    // }

    uint64_t distTravelledSinceLastSensorHit = ((int64_t)velocityEstimate * timeSinceLastSensorHit) / 1000000000;  // mm

    distRemainingToSensorAhead = ((distanceToSensorAhead > distTravelledSinceLastSensorHit)
                                      ? (distanceToSensorAhead - distTravelledSinceLastSensorHit)
                                      : 0);

    distRemainingToZoneEntranceSensorAhead = ((distanceToZoneEntraceSensorAhead > distTravelledSinceLastSensorHit)
                                                  ? (distanceToZoneEntraceSensorAhead - distTravelledSinceLastSensorHit)
                                                  : 0);

    // if our stop will end up in the next zone, reserve it
    // fix this wack ass logic with flags
    if (!stopped) {
        printer_proprietor::updateTrainZoneDistance(printerProprietorTid, trainIndex,
                                                    distRemainingToZoneEntranceSensorAhead - stoppingDistance);
    }

    if (distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !stopped &&
        !(zoneEntraceSensorAhead == targetSensor)) {
        ASSERT(stoppingDistance > 0);

        attemptReservation(curMicros);
    }

    if (distanceToSensorAhead > 0 && !slowingDown && !stopped) {
        printer_proprietor::updateTrainDistance(printerProprietorTid, trainIndex, distRemainingToSensorAhead);
    } else if ((curMicros - slowingDown) / (1000 * 1000) > 5 && slowingDown) {
        // we need some slowing logic here
        slowingDown = 0;
        totalStoppingTime = 0;
        stopped = true;
        prevSensorHitMicros = 0;
        prevNotificationMicros = curMicros;
        localization_server::updateReservation(parentTid, trainIndex, reservedZones, ReservationType::STOPPED);
        printer_proprietor::debugPrintF(printerProprietorTid, "Assume we have updated our reservation types");

    } else if (stopped && !(zoneEntraceSensorAhead == targetSensor)) {  // this is wrong, should be zone
        // try and make a reservation request again. If it works, go back to your speed?
        ASSERT(!slowingDown, "we have stopped but 'stopping' has a timestamp");

        if (!attemptReservation(curMicros)) {
            clock_server::Delay(clockServerTid, 10);  // try again in 10 ticks
        }

    } else if (stopped) {
        // we are stopped, generate a new stop location
        prevNotificationMicros = curMicros;
        ASSERT(!slowingDown, "we have stopped but 'stopping' has a timestamp");
        targetSensor.box = 0;
        targetSensor.num = 0;
        // get new destination, which will bring us back
        localization_server::newDestination(parentTid, trainIndex);
        // then set the train speed
        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_SPEED_8, myTrainNumber);

        return;  // DO NOT REPLY TO NOTIFIER
    }
    // otherwise, we are slowing down

    tryToFreeZones(distTravelledSinceLastSensorHit);

    prevNotificationMicros = curMicros;
    emptyReply(notifierTid);  // reply to our notifier HERE
}

void Train::tryToFreeZones(uint64_t distTravelledSinceLastSensorHit) {
    // if you had an old zone that should have been freed (is this still needed?)
    if (!zoneExits.empty() && !(zoneExits.front()->sensorMarkingExit == sensorBehind)) {
        // check
        while (!zoneExits.empty() && !(zoneExits.front()->sensorMarkingExit == sensorBehind)) {
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
                    printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
                    break;
                }
                default: {
                    printer_proprietor::debugPrintF(printerProprietorTid,
                                                    "%s (Backup) Freed Reservation FAILED with sensor %c%d",
                                                    trainColour, reservationSensor.box, reservationSensor.num);
                    break;
                }
            }
        }
    } else if (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit == sensorBehind &&
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
                printer_proprietor::debugPrintF(printerProprietorTid, "%s Freed Reservation FAILED with sensor %c%d",
                                                trainColour, reservationSensor.box, reservationSensor.num);
                break;
            }
        }
    }
}

void Train::reverseNotifier(train_server::Command command, uint64_t clientTid) {
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
            isForward = !isForward;
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
}

void Train::stopNotifier() {
    printer_proprietor::debugPrintF(printerProprietorTid, "STOP NOTIFIER GOT TO US");
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
    totalStoppingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
    slowingDown = timerGet();
    // these should not be zero until we actually stop
    stopSensor.box = 0;
    stopSensor.num = 0;
}

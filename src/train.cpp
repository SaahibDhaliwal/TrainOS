#include "train.h"

#include <algorithm>
#include <cmath>
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
#include "train_data.h"
#include "train_protocol.h"
#include "util.h"

using namespace train_server;
#define TASK_MSG_SIZE 20
#define STOPPING_THRESHOLD 10     // 1 cm
#define TRAIN_SENSOR_TIMEOUT 300  // 5 sec?

// this will need to be a variable and changed when we do reverses
#define DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN 200

Train::Train(unsigned int myTrainNumber, int parentTid, uint32_t printerProprietorTid, uint32_t marklinServerTid,
             uint32_t clockServerTid, uint32_t updaterTid, uint32_t stopNotifierTid)
    : myTrainNumber(myTrainNumber),
      parentTid(parentTid),
      printerProprietorTid(printerProprietorTid),
      marklinServerTid(marklinServerTid),
      clockServerTid(clockServerTid),
      updaterTid(updaterTid),
      stopNotifierTid(stopNotifierTid),
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
    initializeDistanceMatrix(track, distanceMatrix);
}

uint32_t Train::getReverseTid() {
    return reverseTid;
}

Sensor Train::getStopSensor() {
    return stopSensor;
}

void Train::setTrainSpeed(unsigned int trainSpeed) {
    if (!isReversing && !isSlowingDown) {
        printer_proprietor::debugPrintF(printerProprietorTid, "%s Setting speed to %u ", trainColour, trainSpeed);

        speed = trainSpeed;
        stoppingDistance = stopDistFromSpeed(trainIndex, trainSpeed);

        if (trainSpeed == TRAIN_STOP) {
            decelerateTrain();
        } else {
            accelerateTrain();
        }
    } else {
        printer_proprietor::debugPrintF(printerProprietorTid,
                                        "%s FAILED to set speed to %u. reversing: %u slowingDown: %u ", trainColour,
                                        trainSpeed, isReversing, isSlowingDown);
    }
}

uint64_t uint64_sqrt(uint64_t x) {
    if (x == 0 || x == 1) return x;

    uint64_t low = 1, high = x / 2 + 1;
    uint64_t ans = 0;

    while (low <= high) {
        uint64_t mid = low + (high - low) / 2;
        uint64_t midSquared = x / mid;

        if (mid <= midSquared) {
            ans = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return ans;
}

uint64_t Train::predictNextSensorHitTimeMicros() {
    uint64_t curMicros = timerGet();
    uint64_t timeSinceAccelerationStart = curMicros - accelerationStartTime;

    uint64_t a = getAccelerationSeed(trainIndex);             // mm/s² × 1000
    uint64_t v_target = getStoppingVelocitySeed(trainIndex);  // mm/s × 1000

    uint64_t v0 = (a * timeSinceAccelerationStart) / 1000000;  // mm/s × 1000

    if (timeSinceAccelerationStart >= totalAccelerationTime) {  // already finished accelerating
        velocityEstimate = v_target;
        return curMicros + ((distanceToSensorAhead * 1000 * 1000000) / velocityEstimate);
    } else {
        // Predict v1 = velocity at sensor if we keep accelerating the whole way
        uint64_t v0_sq = v0 * v0;  // OVERFLOW CONCERNS
        uint64_t accel_term = 2 * a * distanceToSensorAhead;
        uint64_t v1 = uint64_sqrt(v0_sq + accel_term);  // mm/s × 1000

        if (v1 <= v_target) {  // accelerating through this sensor to next
            uint64_t v_avg = (v0 + v1) / 2;
            return curMicros + ((distanceToSensorAhead * 1000 * 1000000) / v_avg);
        } else {  // will finish accelerating between the sensors
            // time to reach v_target
            uint64_t t_accel = ((v_target - v0) * 1000000) / a;  // µs

            // distance during acceleration
            uint64_t v_avg_accel = (v0 + v_target) / 2;
            uint64_t d_accel = (v_avg_accel * t_accel) / 1000000;  // mm

            // remaining distance to cruise
            uint64_t d_cruise = distanceToSensorAhead - d_accel;
            uint64_t t_cruise = (d_cruise * 1000 * 1000000) / v_target;  // µs

            return curMicros + t_accel + t_cruise;
        }
    }
}

void Train::initialSensorHit(Sensor curSensor) {
    int curSensorIdx = trackNodeIdxFromSensor(curSensor);

    isStopped = false;

    // reserve current zone on initial sensor hit
    char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    localization_server::initReservation(parentTid, trainIndex, curSensor, replyBuff);

    switch (replyFromByte(replyBuff[0])) {
        case Reply::RESERVATION_SUCCESS: {
            zoneEntraceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
            printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex, zoneEntraceSensorAhead);

            int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);

            // we also get distance back in our message though?
            distanceToZoneEntraceSensorAhead = distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];
            // printer_proprietor::debugPrintF(printerProprietorTid, "%s (init) distanceToZoneEntraceSensorAhead: %u ",
            //                                 trainColour, distanceToZoneEntraceSensorAhead);

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
    printer_proprietor::updateTrainStatus(printerProprietorTid, trainIndex, true);  // train active

    sensorAheadMicros = predictNextSensorHitTimeMicros();

    printer_proprietor::updateSensor(printerProprietorTid, curSensor, 999, 999);
}

void Train::regularSensorHit(uint64_t curMicros, Sensor curSensor) {
    int curSensorIdx = trackNodeIdxFromSensor(curSensor);

    // if we came to a stop and chose to start moving again, we should attempt to reserve the next zone then
    if (isStopped) {
        isStopped = false;
        // anything else here?
    }

    int zoneEntraceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntraceSensorAhead);
    distanceToZoneEntraceSensorAhead = distanceMatrix[curSensorIdx][zoneEntraceSensorAheadIdx];

    uint64_t microsDeltaT = curMicros - prevSensorHitMicros;
    // the amount of distance travelled from the last sensor hit to now.
    uint64_t mmDeltaD = prevDistance;
    prevDistance = distanceToSensorAhead;  // this used to be "distance"

    // only use sensor hit to update velocity estimate if we aren't accelerating/decelerating
    if (!isAccelerating && !isSlowingDown) {
        uint64_t nextSample = (mmDeltaD * 1000000) * 1000 / microsDeltaT;  // microm/micros with a *1000 for decimals
        uint64_t lastEstimate = velocityEstimate;
        velocityEstimate = ((lastEstimate * 15) + nextSample) >> 4;  // alpha = 1/8
        printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
    }

    // when did we plan on hitting this sensor vs when did we actually hit it
    prevSensorPredicitionMicros = sensorAheadMicros;
    sensorAheadMicros = predictNextSensorHitTimeMicros();

    int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
    int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
    // why might this always be zero?
    int64_t estimateDistanceDiffmm = ((int64_t)velocityEstimate * estimateTimeDiffmicros) / 1000000000;

    printer_proprietor::updateSensor(printerProprietorTid, curSensor, estimateTimeDiffms, estimateDistanceDiffmm);
    printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, sensorAhead);

    while (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit.box == 'F') {
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
                                                "%s (Backup) Freed Reservation FAILED with sensor %c%d", trainColour,
                                                reservationSensor.box, reservationSensor.num);
                break;
            }
        }
    }
}

void Train::processSensorHit(Sensor curSensor, Sensor newSensorAhead, uint64_t distance) {
    int64_t curMicros = timerGet();

    if (prevSensorHitMicros != 0 && sensorAhead.box != 'F') {
        ASSERT(curSensor.box == sensorAhead.box && curSensor.num == sensorAhead.num,
               "Train %u was not expecting this sensor to come next");
    }

    distanceToSensorAhead = distance;
    sensorAhead = newSensorAhead;

    if (!prevSensorHitMicros) {
        initialSensorHit(curSensor);
        emptyReply(updaterTid);  // ready for real-time train updates now
    } else {
        regularSensorHit(curMicros, curSensor);  // tries to free zones here
    }

    prevSensorHitMicros = curMicros;
    sensorBehind = curSensor;
}

uint64_t Train::getStopDelayTicks(int64_t curMicros) {
    // uint64_t curMicros = timerGet();
    uint64_t arrivalTime = (stopSensorOffset * 1000 * 1000000 / velocityEstimate);
    uint64_t numOfTicks = (arrivalTime) / Config::TICK_SIZE;

    return numOfTicks;
}

void Train::reverseCommand() {
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
    reverseTid = sys::Create(40, &marklin_server::reverseTrainTask);

    // should we change is reversing to one bool and have slowing
    isSlowingDown = true;
    isReversing = true;

    reversingStartTime = timerGet();
    totalReversingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
    printer_proprietor::debugPrintF(printerProprietorTid, "%s TOTAL REVERSE TIME %u", trainColour, totalReversingTime);
}

void Train::newStopLocation(Sensor newStopSensor, Sensor newTargetSensor, uint64_t offset) {
    stopSensor = newStopSensor;
    targetSensor = newTargetSensor;
    stopSensorOffset = offset;

    printer_proprietor::updateTrainDestination(printerProprietorTid, trainIndex, targetSensor);
}

void Train::updateVelocityWithAcceleration(int64_t curMicros) {
    uint64_t timeSinceAccelerationStart = curMicros - accelerationStartTime;
    if (timeSinceAccelerationStart >= totalAccelerationTime) {
        velocityEstimate = velocityFromSpeed(trainIndex, speed);
        isAccelerating = false;
    } else {
        velocityEstimate = (getAccelerationSeed(trainIndex) * timeSinceAccelerationStart) / 1000000;
    }
    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
}

void Train::updateVelocityWithDeceleration(int64_t curMicros) {
    uint64_t timeSinceDecelerationStart = curMicros - stopStartTime;
    uint64_t deltaV = (getDecelerationSeed(trainIndex) * timeSinceDecelerationStart) / 1000000;  // mm/ms

    if (deltaV >= velocityBeforeSlowing) {
        localization_server::updateReservation(parentTid, trainIndex, reservedZones, ReservationType::STOPPED);
        velocityEstimate = 0;
        // stoppingDistance = 0;
        isSlowingDown = false;
        isStopped = true;
    } else {
        velocityEstimate = velocityBeforeSlowing - deltaV;

        // uint64_t term1 = velocityBeforeSlowing * timeSinceDecelerationStart;
        // uint64_t term2 =
        //     (getDecelerationSeed(trainIndex) * timeSinceDecelerationStart * timeSinceDecelerationStart) /
        //     2000000;

        // if (term2 > term1) {
        //     stoppingDistance = 0;
        // } else {
        //     stoppingDistance = (term1 - term2) / 1000000;
        // }

        // printer_proprietor::debugPrintF(printerProprietorTid, "STOPPING DISTANCE IS %u", stoppingDistance);
    }
    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
}

bool Train::attemptReservation(int64_t curMicros) {
    char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    // printer_proprietor::debugPrintF(
    //     printerProprietorTid, "%s \033[48;5;22m Reservation w zoneEntraceSensorAhead: %c%d targetSensor: %c%d",
    //     trainColour, zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num, targetSensor.box, targetSensor.num);
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

            if (isSlowingDown) {
                isSlowingDown = false;
                // do we accelerate from zero? or any speed?
                accelerateTrain();
                marklin_server::setTrainSpeed(marklinServerTid, speed, myTrainNumber);
            }

            firstReservationFailure = false;
            return true;
            break;
        }
        case Reply::RESERVATION_FAILURE: {
            // need to break NOW
            if (!isSlowingDown) {
                // this assumes we cannot already be stopped
                if (!firstReservationFailure) {
                    decelerateTrain();

                    printer_proprietor::debugPrintF(
                        printerProprietorTid,
                        "%s \033[5m \033[38;5;160m FAILED Reservation for zone %d with sensor: %c%d \033[m",
                        trainColour, replyBuff[1], zoneEntraceSensorAhead.box, zoneEntraceSensorAhead.num);

                    firstReservationFailure = true;
                }
            }

            // try again in a bit? after we stopped?
            return false;
            break;
        }
        default: {
            printer_proprietor::debugPrintF(printerProprietorTid, "%s INVALID REPLY FROM makeReservation %d",
                                            trainColour, replyFromByte(replyBuff[0]));
            return false;
            break;
        }
    }
}

void Train::accelerateTrain() {
    isAccelerating = true;
    accelerationStartTime = timerGet();
    isSlowingDown = false;
    isStopped = false;

    uint64_t targetVelocity = velocityFromSpeed(trainIndex, speed);  // mm/s × 1000
    uint64_t acceleration = getAccelerationSeed(trainIndex);         // mm/s² × 1000

    if (velocityEstimate >= targetVelocity) {
        totalAccelerationTime = 0;
        isAccelerating = false;
        return;
    }

    uint64_t deltaV = targetVelocity - velocityEstimate;  // mm/s × 1000
    totalAccelerationTime = (deltaV * 1000000) / acceleration;

    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_SPEED_8, myTrainNumber);
}

void Train::decelerateTrain() {
    if (velocityEstimate == 0) {
        totalStoppingTime = 0;
        isSlowingDown = false;
        isStopped = true;
        return;
    }

    totalStoppingTime = (velocityEstimate * 1000000) / getDecelerationSeed(trainIndex);
    stopStartTime = timerGet();
    velocityBeforeSlowing = velocityEstimate;
    isSlowingDown = true;
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
}

void Train::updateState() {
    int64_t curMicros = timerGet();                                     // micros
    uint64_t timeSinceLastSensorHit = curMicros - prevSensorHitMicros;  // micros
    uint64_t timeSinceLastStateUpdate = curMicros - prevUpdateMicros;

    if (prevUpdateMicros == 0) {
        timeSinceLastStateUpdate = curMicros - prevSensorHitMicros;
    }

    // have some timeout here
    // if ((curMicros - prevSensorPredicitionMicros) / 1000 > TRAIN_SENSOR_TIMEOUT && newSensorsPassed >= 5) {
    //     // do some kind of reset thing
    //     stopped = true;
    //     continue;
    // }

    // this only updates for accelerating from a stop, not slowing down
    if (isAccelerating) {
        Train::updateVelocityWithAcceleration(curMicros);
    }

    if (isSlowingDown) {
        Train::updateVelocityWithDeceleration(curMicros);
    }

    if (isSlowingDown || isAccelerating) {  // TODO: !isSlowingDown?
        distTravelledSinceLastSensorHit += ((int64_t)velocityEstimate * timeSinceLastStateUpdate) / 1000000000;  // mm
    } else {
        distTravelledSinceLastSensorHit = ((int64_t)velocityEstimate * timeSinceLastSensorHit) / 1000000000;  // mm
    }

    // distRemainingToSensorAhead = ((distanceToSensorAhead > distTravelledSinceLastSensorHit)
    //                                   ? (distanceToSensorAhead - distTravelledSinceLastSensorHit)
    //                                   : 0);
    distRemainingToSensorAhead = (int64_t)distanceToSensorAhead - (int64_t)distTravelledSinceLastSensorHit;
    // ASSERT(distRemainingToSensorAhead >= 0);

    distRemainingToZoneEntranceSensorAhead = ((distanceToZoneEntraceSensorAhead > distTravelledSinceLastSensorHit)
                                                  ? (distanceToZoneEntraceSensorAhead - distTravelledSinceLastSensorHit)
                                                  : 0);

    if (!isStopped) {
        printer_proprietor::updateTrainZoneDistance(printerProprietorTid, trainIndex,
                                                    distRemainingToZoneEntranceSensorAhead - stoppingDistance);
    }

    if (zoneEntraceSensorAhead == targetSensor &&
        distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance) {
        decelerateTrain();
    }

    // if our stop will end up in the next zone, reserve it
    // only try to reserve if we are not stopped AND we haven't reached our target sensor yet?
    // what if we are stopped but our target is not next?
    if (distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !isStopped &&
        !(zoneEntraceSensorAhead == targetSensor)) {
        ASSERT(stoppingDistance >= 0);

        bool res = attemptReservation(curMicros);
        if (res && isSlowingDown) {
            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s \033[5m \033[38;5;160m SPEEDING BACK UP WHEN SLOWING DOWN \033[m",
                                            trainColour);
            accelerateTrain();
        }
    }

    if (distanceToSensorAhead > 0 && !isSlowingDown && !isStopped) {
        printer_proprietor::updateTrainDistance(printerProprietorTid, trainIndex, distRemainingToSensorAhead);
    } else if (isStopped && !(zoneEntraceSensorAhead == targetSensor)) {
        // try and make a reservation request again. If it works, go back to your speed?
        ASSERT(!isSlowingDown, "we have stopped but 'stopping' has a timestamp");

        // this would happen
        if (!attemptReservation(curMicros)) {
            clock_server::Delay(clockServerTid, 10);  // try again in 10 ticks
        } else {
            // we are hitting this twice, Why?
            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s \033[5m \033[38;5;160m SPEEDING BACK UP FROM STOP \033[m", trainColour);
            accelerateTrain();
            sensorAheadMicros = timerGet() + 500;  // hardcoded lol (ideally we always stop the same distance away from
                                                   // our target so we should know what this is)
        }
    } else if (isStopped) {
        // we are stopped at our target, generate a new stop location
        // prevNotificationMicros = curMicros;
        ASSERT(!isSlowingDown, "we have stopped but 'stopping' has a timestamp");
        targetSensor.box = 0;
        targetSensor.num = 0;
        // get new destination, which will bring us back
        printer_proprietor::debugPrintF(printerProprietorTid, "%s YO I THINK IM STOPPED", trainColour);
        localization_server::newDestination(parentTid, trainIndex);
        // isStopped = false;

        accelerateTrain();
    }
    // otherwise, we are slowing down

    tryToFreeZones(distTravelledSinceLastSensorHit);
    prevUpdateMicros = curMicros;
}

void Train::tryToFreeZones(uint64_t distTravelledSinceLastSensorHit) {
    // check

    if (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit == sensorBehind &&
        (distTravelledSinceLastSensorHit >= DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN)) {  // can we free any zones?
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

uint64_t Train::getReverseDelayTicks() {
    uint64_t curTime = timerGet();
    uint64_t timeToStop = (totalReversingTime - (curTime - reversingStartTime)) / 1000;  // millis
    uint64_t ticksToStop = (timeToStop + 9) / 10;                                        // round up
    printer_proprietor::debugPrintF(printerProprietorTid, "%s TOTAL TICKS TO STOP %u", trainColour, ticksToStop);
    return ticksToStop;
}

void Train::finishReverse() {
    ASSERT(reverseTid != 0);
    ASSERT(isReversing);

    marklin_server::setTrainReverseAndSpeed(marklinServerTid, speed, myTrainNumber);
    isReversing = false;
    isForward = !isForward;

    printer_proprietor::updateTrainOrientation(printerProprietorTid, trainIndex, isForward);

    if (!isForward) {
        // stopping distance is the distance from sensor bar to front of train + front of train to stop
        // isReversing would mean we need to subtract this distance ^
        // from the distance of the back of the train to the sensor bar
        // back of train to sensor bar: 14.5 cm
        stoppingDistance += 115;
    } else {
        stoppingDistance -= 115;  // TODO: Why subtract here?
    }

    accelerateTrain();
}

void Train::stop() {
    printer_proprietor::debugPrintF(printerProprietorTid, "%s STOP NOTIFIER GOT TO US", trainColour);
    decelerateTrain();

    // these should not be zero until we actually stop
    stopSensor.box = 0;
    stopSensor.num = 0;
}

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
#include "reservation_server_protocol.h"
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
#define DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN 175
#define REVERSED_TRAIN_DISTANCE_FROM_NEW_FRONT_TO_SENSOR_BAR 150

Train::Train(unsigned int myTrainNumber, int parentTid, uint32_t printerProprietorTid, uint32_t marklinServerTid,
             uint32_t clockServerTid, uint32_t updaterTid, uint32_t stopNotifierTid, uint32_t reservationTid)
    : myTrainNumber(myTrainNumber),
      parentTid(parentTid),
      printerProprietorTid(printerProprietorTid),
      marklinServerTid(marklinServerTid),
      clockServerTid(clockServerTid),
      updaterTid(updaterTid),
      stopNotifierTid(stopNotifierTid),
      reservationTid(reservationTid),
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
    trainReservation.initialize(track, printerProprietorTid);
}

uint32_t Train::getReverseTid() {
    return reverseTid;
}

Sensor Train::getStopSensor() {
    return stopSensor;
}

void Train::waitForReservation(Sensor reservationSensor) {
    char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    int reservationServerTid = name_server::WhoIs(RESERVATION_SERVER_NAME);
    ASSERT(reservationServerTid >= 0, "UNABLE TO GET RESERVATION_SERVER_NAME\r\n");
    reservation_server::makeReservation(reservationServerTid, trainIndex, reservationSensor, replyBuff);

    int64_t curTime = timerGet();
    bool firstUpdate = true;
    while (reservation_server::replyFromByte(replyBuff[0]) != reservation_server::Reply::RESERVATION_SUCCESS) {
        replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};  // clear it?
        clock_server::Delay(clockServerTid, 50);
        reservation_server::makeReservation(reservationServerTid, trainIndex, reservationSensor, replyBuff);
        int64_t newDelayTime = timerGet();
        if (newDelayTime - curTime >= 1000000 * 2) {
            localization_server::UpdateResInfo updateResInfo =
                localization_server::updateReservation(parentTid, trainIndex, reservedZones, ReservationType::STOPPED);
            printer_proprietor::debugPrintF(
                printerProprietorTid,
                "%s (BUSY WAIT FOR RESERVATION) update reservation got back to me with has new path: %d", trainColour,
                updateResInfo.hasNewPath);

            if (updateResInfo.hasNewPath) {
                newStopLocation(updateResInfo.destInfo.stopSensor, updateResInfo.destInfo.targetSensor,
                                updateResInfo.destInfo.firstSensor, updateResInfo.destInfo.distance,
                                updateResInfo.destInfo.reverse);
            }
            //     localization_server::updateReservation(parentTid, trainIndex, reservedZones,
            //     ReservationType::STOPPED);;
        }
        // if this fails enough times
        //  update reservations
    }

    uint64_t distToExitSensor = distRemainingToZoneEntranceSensorAhead + DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN;

    ReservedZone reservation{.sensorMarkingEntrance = reservationSensor, .zoneNum = static_cast<uint8_t>(replyBuff[1])};
    reservedZones.push(reservation);

    // let's create an exit entry for a previous zone
    ZoneExit zoneExit{.sensorMarkingExit = reservationSensor, .distanceToExitSensor = distToExitSensor};
    zoneExits.push(zoneExit);

    // we have a new zone entrace sensor we care about
    zoneEntranceSensorAhead = Sensor{.box = replyBuff[2], .num = static_cast<uint8_t>(replyBuff[3])};
    printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex, zoneEntranceSensorAhead);

    if (sensorBehind == reservationSensor) {
        sensorAhead = zoneEntranceSensorAhead;
    }

    unsigned int distance = 0;  // TODO: we are casting to unsigned int, when we have a uint64_t
    a2ui(&replyBuff[4], 10, &distance);

    // printer_proprietor::debugPrintF(printerProprietorTid,
    //                                 "%s \033[48;5;22m Made Reservation for Zone: %d using Zone Entrance "
    //                                 "Sensor: %c%d because I was %u mm away. Next zone "
    //                                 "entrance sensor is %c%d which is %u mm away",
    //                                 trainColour, replyBuff[1], zoneExit.sensorMarkingExit.box,
    //                                 zoneExit.sensorMarkingExit.num, distRemainingToZoneEntranceSensorAhead,
    //                                 zoneEntranceSensorAhead.box, zoneEntranceSensorAhead.num,
    //                                 distanceToZoneEntranceSensorAhead + distance);

    int zoneEntranceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntranceSensorAhead);
    int sensorAheadIdx = trackNodeIdxFromSensor(sensorAhead);
    distanceToZoneEntranceSensorAhead =
        distanceMatrix[sensorAheadIdx][zoneEntranceSensorAheadIdx] + distanceToSensorAhead;

    recentZoneAddedFlag = true;
    printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
    firstReservationFailure = false;
}

void Train::setTrainSpeed(unsigned int trainSpeed) {
    if (!isReversing && !isSlowingDown) {
        speed = trainSpeed;
        stoppingDistance = stopDistFromSpeed(trainIndex, trainSpeed);

        if (trainSpeed == TRAIN_STOP) {
            decelerateTrain();
        } else if (!firstReservationFailure) {
            accelerateTrain();
        } else {
            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s BLOCKED PLAYER INPUT to set speed to %u since firstReservationFailure "
                                            "is TRUE \n\r zone entrance sensor ahead is %c%d",
                                            trainColour, trainSpeed, zoneEntranceSensorAhead.box,
                                            zoneEntranceSensorAhead.num);
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
    isStopped = false;

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

    int zoneEntranceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntranceSensorAhead);
    distanceToZoneEntranceSensorAhead = distanceMatrix[curSensorIdx][zoneEntranceSensorAheadIdx];

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
    // removed freeing zones that start with F on sensor hit
    // we should be freeing those as we travel anyways
}

void Train::processSensorHit(Sensor curSensor, Sensor newSensorAhead, uint64_t distance) {
    int64_t curMicros = timerGet();

    if (prevSensorHitMicros != 0 && sensorAhead.box != 'F') {
        if (!(curSensor.box == sensorAhead.box) || !(curSensor.num == sensorAhead.num) && !isPlayer) {
            printer_proprietor::debugPrintF(printerProprietorTid, "The cursensor is %c%d and sensorAhead is %c%d",
                                            curSensor.box, curSensor.num, newSensorAhead.box, newSensorAhead.num);
            clock_server::Delay(clockServerTid, 100);
            ASSERT(curSensor.box == sensorAhead.box && curSensor.num == sensorAhead.num,
                   "Train %u was not expecting this sensor to come next. We expected sensor %c%d but got sensor %c%d",
                   myTrainNumber, sensorAhead.box, sensorAhead.num, curSensor.box, curSensor.num);
        }
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
    if (isStopped) {
        // printer_proprietor::debugPrintF(printerProprietorTid, "IN HERE PAPA NUMERO 1");
        finishReverse();
    } else {
        // printer_proprietor::debugPrintF(printerProprietorTid, "IN HERE PAPA NUMERO 2");

        decelerateTrain();
        isReversing = true;
    }
}

void Train::newStopLocation(Sensor newStopSensor, Sensor newTargetSensor, Sensor firstSensor, uint64_t offset,
                            bool reverse) {
    stopSensor = newStopSensor;
    targetSensor = newTargetSensor;
    stopSensorOffset = offset;

    printer_proprietor::updateTrainDestination(printerProprietorTid, trainIndex, targetSensor);

    if (reverse) {
        printer_proprietor::debugPrintF(printerProprietorTid, "REVERSE THAT SHIT");

        marklin_server::setTrainReverse(marklinServerTid, myTrainNumber);
        isForward = !isForward;

        printer_proprietor::updateTrainOrientation(printerProprietorTid, trainIndex, isForward);

        if (!isForward) {
            // stopping distance is the distance from sensor bar to front of train + front of train to stop
            // isReversing would mean we need to subtract this distance ^
            // from the distance of the back of the train to the sensor bar
            // back of train to sensor bar: 14.5 cm
            stoppingDistance += 120;
        } else {
            stoppingDistance -= 120;  // TODO: Why subtract here?
        }

        int initialReservedZonesNum = reservedZones.size();
        int initialZoneExitsNum = zoneExits.size();
        TrackNode* sensorAheadNodeReversed = track[trackNodeIdxFromSensor(sensorAhead)].reverse;
        int startIdx = sensorAheadNodeReversed->id;

        sensorAhead = firstSensor;  // this helps with reservations

        int newSensorAheadIdx = trackNodeIdxFromSensor(sensorAhead);

        distanceToSensorAhead =
            distanceMatrix[startIdx][newSensorAheadIdx] - REVERSED_TRAIN_DISTANCE_FROM_NEW_FRONT_TO_SENSOR_BAR;

        printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, sensorAhead);

        printer_proprietor::debugPrintF(printerProprietorTid, "ATTEMPTING RESERVATIONS");

        RingBuffer<ReservedZone, 32> newReservedZones;

        TrackNode* finalZoneEntranceSensorNode =
            track[trackNodeIdxFromSensor(reservedZones.begin()->sensorMarkingEntrance)].reverse;

        // FIRST RESERVATION
        char tarBox = sensorAheadNodeReversed->name[0];
        unsigned int tarNum = ((sensorAheadNodeReversed->num + 1) - (tarBox - 'A') * 16);
        Sensor reservationSensor = {.box = tarBox, .num = tarNum};
        if (reservationSensor.box == 'F') {
            reservationSensor.num = sensorAheadNodeReversed->num;
        }

        ReservedZone* latestZone = reservedZones.back();
        newReservedZones.push(ReservedZone{.sensorMarkingEntrance = reservationSensor, .zoneNum = latestZone->zoneNum});

        sensorBehind = reservationSensor;

        auto it = reservedZones.end() - 1;  // start from end

        for (int i = 0; i < initialReservedZonesNum - 1; i += 1, --it) {
            Sensor reservationSensor = (*it).sensorMarkingEntrance;

            TrackNode* reservationSensorNode = track[trackNodeIdxFromSensor(reservationSensor)].reverse;

            Sensor newReservationSensor = {
                .box = reservationSensorNode->name[0],
                .num = (reservationSensorNode->num + 1) - (reservationSensorNode->name[0] - 'A') * 16};

            if (newReservationSensor.box == 'F') {
                newReservationSensor.num = reservationSensorNode->num;
            }

            for (auto it2 = newReservedZones.begin(); it2 != newReservedZones.end(); ++it2) {
                if ((*it2).sensorMarkingEntrance == newReservationSensor) {
                    printer_proprietor::debugPrintF(printerProprietorTid,
                                                    "YOOOOOOOOOOOOOOOOO WE ARE PUSHING A DUPE SENSOR %c%u",
                                                    newReservationSensor.box, newReservationSensor.num);
                }

                if ((*it2).zoneNum == (*it).zoneNum) {
                    printer_proprietor::debugPrintF(
                        printerProprietorTid, "YOOOOOOOOOOOOOOOOO WE ARE PUSHING A DUPE ZONE NUM  %u", (*it).zoneNum);
                }
            }

            newReservedZones.push(
                {.sensorMarkingEntrance = newReservationSensor,
                 .zoneNum = static_cast<uint8_t>(trainReservation.trackNodeToZoneNum(reservationSensorNode))});
        }

        ASSERT(newReservedZones.size() == reservedZones.size());

        for (auto it = newReservedZones.begin(); it != newReservedZones.end(); ++it) {
            reservedZones.pop();
            reservedZones.push(*it);
        }

        while (!zoneExits.empty()) {
            zoneExits.pop();
        }

        it = reservedZones.begin();
        Sensor prevSensor = reservedZones.begin()->sensorMarkingEntrance;
        uint64_t distanceToExit = 0;
        ++it;

        for (it; it != reservedZones.end(); ++it) {  // everything except last reserved zone has a zone exit
            Sensor exitSensor = (*it).sensorMarkingEntrance;
            int prevSensorIdx = trackNodeIdxFromSensor(prevSensor);
            int exitSensorIdx = trackNodeIdxFromSensor(exitSensor);
            if (distanceToExit == 0) {
                distanceToExit =
                    distanceMatrix[prevSensorIdx][exitSensorIdx] - REVERSED_TRAIN_DISTANCE_FROM_NEW_FRONT_TO_SENSOR_BAR;
            } else {
                distanceToExit += distanceMatrix[prevSensorIdx][exitSensorIdx];
            }

            zoneExits.push(ZoneExit{
                .sensorMarkingExit = exitSensor,
                .distanceToExitSensor = distanceToExit,
                .zoneNum = (*it).zoneNum,
            });
        }

        printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);

        Sensor newZoneEntranceSensorAhead = {
            .box = finalZoneEntranceSensorNode->name[0],
            .num = (finalZoneEntranceSensorNode->num + 1) - (finalZoneEntranceSensorNode->name[0] - 'A') * 16};

        if (newZoneEntranceSensorAhead.box == 'F') {
            newZoneEntranceSensorAhead.num = finalZoneEntranceSensorNode->num;
        }

        zoneEntranceSensorAhead = newZoneEntranceSensorAhead;

        distanceToZoneEntranceSensorAhead = distanceToExit;
        waitForReservation(zoneEntranceSensorAhead);
        accelerateTrain();
        sensorAheadMicros =
            predictNextSensorHitTimeMicros();  // hardcoded lol (ideally we always stop the same distance away
        // from our target so we should know what this is)
    } else {
        waitForReservation(zoneEntranceSensorAhead);
        accelerateTrain();
    }
}

void Train::updateVelocityWithAcceleration(int64_t curMicros) {
    uint64_t timeSinceAccelerationStart = curMicros - accelerationStartTime;
    if (timeSinceAccelerationStart >= totalAccelerationTime) {
        velocityEstimate = velocityFromSpeed(trainIndex, speed);
        stoppingDistance = getStoppingDistSeed(trainIndex);
        isAccelerating = false;
    } else {
        velocityEstimate = (getAccelerationSeed(trainIndex) * timeSinceAccelerationStart) / 1000000;
        uint64_t v_ratio_numerator = velocityEstimate;
        uint64_t v_ratio_denominator = getStoppingVelocitySeed(trainIndex);

        uint64_t newStoppingDistance = ((getStoppingDistSeed(trainIndex) * v_ratio_numerator * v_ratio_numerator) /
                                        (v_ratio_denominator * v_ratio_denominator)) +
                                       20;
        if (newStoppingDistance < getStoppingDistSeed(trainIndex)) {
            stoppingDistance = newStoppingDistance;
        }
    }
    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
}

void Train::updateVelocityWithDeceleration(int64_t curMicros) {
    uint64_t timeSinceDecelerationStart = curMicros - stopStartTime;
    uint64_t deltaV = (getDecelerationSeed(trainIndex) * timeSinceDecelerationStart) / 1000000;  // mm/ms

    if (deltaV >= velocityBeforeSlowing) {
        // localization_server::UpdateResInfo updateResInfo =
        //     localization_server::updateReservation(parentTid, trainIndex, reservedZones, ReservationType::STOPPED);
        // printer_proprietor::debugPrintF(
        //     printerProprietorTid,
        //     "%s (INSTANTLY ON TRAIN STOP) update reservation got back to me with has new path: %d", trainColour,
        //     updateResInfo.hasNewPath);

        // if (updateResInfo.hasNewPath) {
        //     newStopLocation(updateResInfo.destInfo.stopSensor, updateResInfo.destInfo.targetSensor,
        //                     updateResInfo.destInfo.firstSensor, updateResInfo.destInfo.distance,
        //                     updateResInfo.destInfo.reverse);
        // }
        velocityEstimate = 0;
        stoppingDistance = 0;
        isSlowingDown = false;
        isStopped = true;

        if (isReversing) {
            finishReverse();
            isReversing = false;
        }
    } else {
        velocityEstimate = velocityBeforeSlowing - deltaV;

        uint64_t v_ratio_numerator = velocityEstimate;
        uint64_t v_ratio_denominator = getStoppingVelocitySeed(trainIndex);

        uint64_t newStoppingDistance = ((getStoppingDistSeed(trainIndex) * v_ratio_numerator * v_ratio_numerator) /
                                        (v_ratio_denominator * v_ratio_denominator)) +
                                       20;
        if (newStoppingDistance < getStoppingDistSeed(trainIndex)) {
            stoppingDistance = newStoppingDistance;
        }
    }
    printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);
}

bool Train::attemptReservation(Sensor reservationSensor) {
    if (isReservationCourierAvailable) {
        reservation_server::courierMakeReservation(reservationTid, reservationSensor);
        lastReservationSensor = reservationSensor;
        return true;
    }
    return false;
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
        printer_proprietor::debugPrintF(printerProprietorTid,
                                        "%s did not set train speed Velocity estimate %u target velocity: %u",
                                        trainColour, velocityEstimate, targetVelocity);

        return;
    }

    uint64_t deltaV = targetVelocity - velocityEstimate;  // mm/s × 1000
    totalAccelerationTime = (deltaV * 1000000) / acceleration;
    printer_proprietor::debugPrintF(printerProprietorTid,
                                    "%s \033[5m \033[38;5;160m SENDING COMMAND TO SPEED BACK UP \033[m", trainColour);
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

    distRemainingToZoneEntranceSensorAhead =
        ((distanceToZoneEntranceSensorAhead > distTravelledSinceLastSensorHit)
             ? (distanceToZoneEntranceSensorAhead - distTravelledSinceLastSensorHit)
             : 0);

    if (!isStopped) {
        printer_proprietor::updateTrainZoneDistance(printerProprietorTid, trainIndex,
                                                    distRemainingToZoneEntranceSensorAhead - stoppingDistance);
    }

    if (zoneEntranceSensorAhead == targetSensor &&
        distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !isSlowingDown) {
        decelerateTrain();
    }

    // if our stop will end up in the next zone, reserve it
    // only try to reserve if we are not stopped AND we haven't reached our target sensor yet?
    // what if we are stopped but our target is not next?
    if (distRemainingToZoneEntranceSensorAhead <= STOPPING_THRESHOLD + stoppingDistance && !isStopped &&
        !(zoneEntranceSensorAhead == targetSensor) && (curMicros - sensorAheadMicros) <= 3 * 1000) {
        ASSERT(stoppingDistance > 0);

        attemptReservation(zoneEntranceSensorAhead);
    }
    // i want to remove checking that we are not slowing down in the future.
    if (distRemainingToSensorAhead && !isSlowingDown && !isStopped) {
        printer_proprietor::updateTrainDistance(printerProprietorTid, trainIndex, distRemainingToSensorAhead);

        // check if we think we've passed a fake sensor in order to tell the localization server
        if (distRemainingToSensorAhead <= 0 && sensorAhead.box == 'F') {
            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s I think I've hit my fake sensor %c%d since my distremaining was %d",
                                            trainColour, sensorAhead.box, sensorAhead.num, distRemainingToSensorAhead);
            localization_server::hitFakeSensor(parentTid, trainIndex);  // localization blocked
        }

    } else if (isStopped && !(zoneEntranceSensorAhead == targetSensor)) {
        // try and make a reservation request again. If it works, go back to your speed?
        ASSERT(!isSlowingDown, "we have stopped but 'stopping' has a timestamp");
        attemptReservation(zoneEntranceSensorAhead);

    } else if (isStopped) {
        // we are stopped at our target, generate a new stop location
        // prevNotificationMicros = curMicros;
        ASSERT(!isSlowingDown, "we have stopped but 'stopping' has a timestamp");
        targetSensor.box = 0;
        targetSensor.num = 0;
        // get new destination, which will bring us back
        printer_proprietor::debugPrintF(printerProprietorTid, "%s YO I THINK IM STOPPED", trainColour);

        localization_server::DestinationInfo destInfo = localization_server::newDestination(parentTid, trainIndex);

        newStopLocation(destInfo.stopSensor, destInfo.targetSensor, destInfo.firstSensor, destInfo.distance,
                        destInfo.reverse);

        printer_proprietor::debugPrintF(printerProprietorTid, "BACK FROM SENDING NEW DESTINATION");
    }
    // otherwise, we are slowing down
    if (!isStopped) {
        tryToFreeZones(distTravelledSinceLastSensorHit);
    }

    prevUpdateMicros = curMicros;
}

void Train::tryToFreeZones(uint64_t distTravelledSinceLastSensorHit) {
    // TODO
    //  can the sensor marking our exit not be the sensor behind?
    //  that is, is there a point inbetween two sensors that is shorter than DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN?
    //  pretty sure yes, because of our fake sensors
    if (!zoneExits.empty() && zoneExits.front()->sensorMarkingExit == sensorBehind &&
        (distTravelledSinceLastSensorHit >= DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN) &&
        isReservationCourierAvailable) {  // can we free any zones?
        lastFreedSensor = zoneExits.front()->sensorMarkingExit;
        reservation_server::courierFreeReservation(reservationTid, lastFreedSensor);
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
    if (!isPlayer) {
        ASSERT(isReversing);  // crashes here
    }

    printer_proprietor::debugPrintF(printerProprietorTid, "REVERSE THAT SHIT");

    marklin_server::setTrainReverse(marklinServerTid, myTrainNumber);
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

    // need to create zoneExits for everything except for the latest zone we reserved (to stop in)

    finishReverseOnNextDest = true;
}

void Train::stop() {
    printer_proprietor::debugPrintF(printerProprietorTid, "%s STOP NOTIFIER GOT TO US", trainColour);
    decelerateTrain();

    // these should not be zero until we actually stop
    stopSensor.box = 0;
    stopSensor.num = 0;
}

void Train::initCPU() {
    // our init should pass us our starting sensor
    // waitForReservation();
    localization_server::DestinationInfo destInfo = localization_server::newDestination(parentTid, trainIndex);
    newStopLocation(destInfo.stopSensor, destInfo.targetSensor, destInfo.firstSensor, destInfo.distance,
                    destInfo.reverse);
    setTrainSpeed(TRAIN_SPEED_8);
}

void Train::initPlayer() {
    // waitForReservation();
    isPlayer = true;
}

void Train::handleReservationReply(char* receiveBuffer) {
    isReservationCourierAvailable = true;
    reservation_server::Reply reply = reservation_server::replyFromByte(receiveBuffer[0]);
    switch (reply) {
        case reservation_server::Reply::RESERVATION_SUCCESS: {
            uint64_t distToExitSensor =
                distRemainingToZoneEntranceSensorAhead + DISTANCE_FROM_SENSOR_BAR_TO_BACK_OF_TRAIN;

            ReservedZone reservation{.sensorMarkingEntrance = lastReservationSensor,
                                     .zoneNum = static_cast<uint8_t>(receiveBuffer[1])};

            for (auto it = reservedZones.begin(); it != reservedZones.end(); ++it) {
                if ((*it).sensorMarkingEntrance == reservation.sensorMarkingEntrance) {
                    printer_proprietor::debugPrintF(
                        printerProprietorTid, "YOOOOOOOOOOOOOOOOO WE ARE PUSHING A DUPE SENSOR %c%u",
                        reservation.sensorMarkingEntrance.box, reservation.sensorMarkingEntrance.num);
                }

                if ((*it).zoneNum == reservation.zoneNum) {
                    printer_proprietor::debugPrintF(printerProprietorTid,
                                                    "YOOOOOOOOOOOOOOOOO WE ARE PUSHING A DUPE ZONE NUM  %u",
                                                    reservation.zoneNum);
                }
            }

            reservedZones.push(reservation);

            // let's create an exit entry for a previous zone
            ZoneExit zoneExit{.sensorMarkingExit = lastReservationSensor,
                              .distanceToExitSensor = distToExitSensor,
                              .zoneNum = static_cast<uint8_t>(receiveBuffer[1])};
            zoneExits.push(zoneExit);

            // note: WE NO LONGER GET THIS
            // since refactor will mean that the train has the path, we should know the next zone entrance sensor

            // we have a new zone entrace sensor we care about
            zoneEntranceSensorAhead = Sensor{.box = receiveBuffer[2], .num = static_cast<uint8_t>(receiveBuffer[3])};
            printer_proprietor::updateTrainZoneSensor(printerProprietorTid, trainIndex, zoneEntranceSensorAhead);

            // I forgot why this is here, maybe on initialization reservation?
            if (sensorBehind == lastReservationSensor) {
                sensorAhead = zoneEntranceSensorAhead;
            }

            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s \033[48;5;22m Made Reservation for Zone: %d using Zone Entrance "
                                            "Sensor: %c%d because I was %u mm away. Next zone "
                                            "entrance sensor is %c%d",
                                            trainColour, receiveBuffer[1], zoneExit.sensorMarkingExit.box,
                                            zoneExit.sensorMarkingExit.num, distRemainingToZoneEntranceSensorAhead,
                                            zoneEntranceSensorAhead.box, zoneEntranceSensorAhead.num);

            int zoneEntranceSensorAheadIdx = trackNodeIdxFromSensor(zoneEntranceSensorAhead);
            int sensorAheadIdx = trackNodeIdxFromSensor(sensorAhead);
            distanceToZoneEntranceSensorAhead =
                distanceMatrix[sensorAheadIdx][zoneEntranceSensorAheadIdx] + distanceToSensorAhead;

            recentZoneAddedFlag = true;
            printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);

            firstReservationFailure = false;

            // newly added during refactor.
            if (isSlowingDown) {
                printer_proprietor::debugPrintF(printerProprietorTid,
                                                "%s \033[5m \033[38;5;160m SPEEDING BACK UP WHEN SLOWING DOWN \033[m",
                                                trainColour);
                accelerateTrain();
            } else if (isStopped) {
                printer_proprietor::debugPrintF(
                    printerProprietorTid, "%s \033[5m \033[38;5;160m SPEEDING BACK UP FROM STOP \033[m", trainColour);
                accelerateTrain();
                sensorAheadMicros = timerGet() + 500;  // hardcoded lol (ideally we always stop the same distance away
                                                       // from our target so we should know what this is)
                firstReservationFailure = true;
            }

            break;
        }
        case reservation_server::Reply::RESERVATION_FAILURE: {
            // need to break NOW
            if (!isSlowingDown && !isStopped) {
                // this assumes we cannot already be stopped
                if (!firstReservationFailure) {
                    decelerateTrain();

                    printer_proprietor::debugPrintF(
                        printerProprietorTid,
                        "%s \033[5m \033[38;5;160m FAILED Reservation for zone %d with sensor: %c%d \033[m",
                        trainColour, receiveBuffer[1], zoneEntranceSensorAhead.box, zoneEntranceSensorAhead.num);

                    firstReservationFailure = true;
                }
            } else if (isStopped) {
                clock_server::Delay(clockServerTid, 10);  // try again in 10 ticks
                int64_t curMicros = timerGet();           // micros
                uint64_t timeSinceDecelerationStart = curMicros - stopStartTime;
                // if it's been two seconds since we've been slowing down, update our reservations
                if (timeSinceDecelerationStart > 1000000 * 2) {
                    reservation_server::courierUpdateReservation(reservationTid, reservedZones,
                                                                 ReservationType::STOPPED);
                    // will need some checking here for deadlocks
                    // our reservation failure will tell us which train is in the reservation we want
                    // so we can send that train a message
                }
            }

            break;
        }
        case reservation_server::Reply::FREE_SUCCESS: {
            printer_proprietor::debugPrintF(printerProprietorTid,
                                            "%s \033[48;5;17m Freed Reservation with zone: %d with sensor: %c%d",
                                            trainColour, receiveBuffer[1], lastFreedSensor.box, lastFreedSensor.num);

            zoneExits.pop();
            reservedZones.pop();
            printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, reservedZones);
            break;
        }
        default: {
            printer_proprietor::debugPrintF(printerProprietorTid, "%s INVALID REPLY FROM makeReservation %d",
                                            trainColour, replyFromByte(receiveBuffer[0]));
            break;
        }
    }
}

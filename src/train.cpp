#include "train.h"

#include "command.h"
#include "marklin_server_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "test_utils.h"

static const int trainAddresses[MAX_TRAINS] = {14, 15, 16, 17, 18, 55};
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
#define REAR_OF_TRAIN -200

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
    // some initial receive from the localization server to know its ID
    char receiveBuffTemp[Config::MAX_MESSAGE_LENGTH] = {0};
    int msgSize = sys::Receive(&clientTid, receiveBuffTemp, Config::MAX_MESSAGE_LENGTH);

    unsigned int myTrainNumber = 0;
    a2ui(receiveBuffTemp, 10, &myTrainNumber);
    emptyReply(clientTid);
    int trainIndex = trainNumToIndex(myTrainNumber);

    // register ID
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

    // and some important info to track

    char boxAhead = 0;
    char sensorNumAhead = 0;

    char boxBehind = 0;
    char sensorNumBehind = 0;

    // train struct info
    // bool stopping = false;
    uint64_t slowingDown = 0;
    bool stopped = true;
    bool reversing = false;
    bool reservationFlag = false;
    bool recentZoneAddedFlag = false;

    uint8_t speed = 0;

    uint64_t prevMicros = 0;
    uint64_t prevDistance = 0;
    int64_t prevSensorPredicitionMicros = 0;
    int64_t sensorAheadMicros = 0;
    uint64_t newSensorsPassed = 0;

    // relative to the next sensor
    int64_t sensorAheadDistance = 0;
    int64_t currentDistancePrediction = 0;
    uint64_t lastPrintTime = 0;

    // our acceleration model should change these
    uint64_t velocityEstimate = 0;
    uint64_t stoppingDistance = 0;

    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;

    // Some kind of ring buffer to show reservations and their zones?
    // RingBuffer<std::pair<char, char>, 16> zoneExitSensor;
    // RingBuffer<std::pair<unsigned int, int>, 16> zoneExitDistance;

    RingBuffer<std::pair<std::pair<char, char>, std::pair<unsigned int, int>>, 16> zoneExits;
    bool zoneStatusChange = false;

    // create notifier for when to print the train status
    uint32_t notifierTid = sys::Create(NOTIFIER_PRIORITY, TrainNotifier);
    receiveBuffTemp[Config::MAX_MESSAGE_LENGTH] = {0};
    sys::Receive(&clientTid, receiveBuffTemp, Config::MAX_MESSAGE_LENGTH);
    ASSERT(clientTid == notifierTid, "train startup received from someone other than the train notifier");

    for (;;) {
        char receiveBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgSize = sys::Receive(&clientTid, receiveBuff, Config::MAX_MESSAGE_LENGTH - 1);
        if (clientTid == parentTid) {
            Command command = commandFromByte(receiveBuff[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    emptyReply(clientTid);  // reply right away to reduce latency
                    unsigned int trainSpeed = 10 * a2d(receiveBuff[1]) + a2d(receiveBuff[2]);

                    // clock_server::Delay(clockServerTid, 10);
                    // ASSERT(0, "trainspeed: %d receiveBuff[1]: %c receiveBuff[2]: %c", trainSpeed);
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
                case Command::NEXT_SENSOR: {
                    // if sensor, process and update velocity...

                    //  process sensor notification and time
                    //  will send us the next sensor and how far away it is
                    //  upon getting this message, we get current time (assume we hit the next sensor?)
                    //  we should have a seperate command byte for if we reached the next sensor or if there was a
                    //  sensor error
                    int64_t curMicros = timerGet();
                    emptyReply(clientTid);  // reply right away to reduce latency
                    char box = receiveBuff[1];
                    int sensorNum = static_cast<int>(receiveBuff[2]);
                    boxAhead = receiveBuff[3];
                    sensorNumAhead = static_cast<int>(receiveBuff[4]);
                    unsigned int distance = 0;  // WRONG? was a uint64_t, now we're casting to unsigned int
                    a2ui(&receiveBuff[5], 10, &distance);
                    sensorAheadDistance = distance;

                    if (!prevMicros) {
                        prevMicros = curMicros;
                        // do some reservation stuff here as well? Since this is our first sensor hit on startup?
                        emptyReply(notifierTid);
                        ASSERT(velocityEstimate != 0, "We should have a seed initialized by now");
                        // make our train active
                        printer_proprietor::updateTrainStatus(printerProprietorTid, trainIndex, true);
                        // reserve the zone this sensor is on
                        char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                        localization_server::makeReservation(parentTid, trainIndex, box, sensorNum, replyBuff);
                        if (replyFromByte(replyBuff[0]) == Reply::RESERVATION_SUCESS) {
                            reservationFlag = true;
                            // std::pair<std::pair<char, char>, std::pair<unsigned int, int>> newZoneExit;
                            // newZoneExit.first.first = box;
                            // newZoneExit.first.second = sensorNum;
                            // newZoneExit.second.first = replyBuff[1];
                            // newZoneExit.second.second = REAR_OF_TRAIN;
                            // zoneExits.push(newZoneExit);
                            // zoneStatusChange = true;

                            char debugBuff[200] = {0};
                            printer_proprietor::formatToString(
                                debugBuff, 200, "[Train %u] Initial Reservation with zone %u with sensor %c%u",
                                myTrainNumber, replyBuff[1], box, sensorNum);
                            printer_proprietor::debug(printerProprietorTid, debugBuff);

                        } else {
                            char debugBuff[200] = {0};
                            printer_proprietor::formatToString(debugBuff, 200,
                                                               "[Train %u] Initial Reservation FAILED with sensor %c%u",
                                                               myTrainNumber, box, sensorNum);
                            printer_proprietor::debug(printerProprietorTid, debugBuff);
                        }

                        boxBehind = box;
                        sensorNumBehind = sensorNum;

                        continue;
                    } else {
                        newSensorsPassed++;
                        uint64_t microsDeltaT = curMicros - prevMicros;
                        uint64_t mmDeltaD = prevDistance;
                        prevDistance = distance;

                        // Using Velocity Samples ---------------------------------------------
                        // if (velocitySamples.empty()) {
                        //     velocitySamplesNumeratorSum = 0;
                        //     velocitySamplesDenominatorSum = 0;
                        // }

                        // if (newSensorsPassed >= 5) {
                        //     if (velocitySamples.full()) {
                        //         std::pair<uint64_t, uint64_t>* p = velocitySamples.front();
                        //         velocitySamplesNumeratorSum -= p->first;
                        //         velocitySamplesDenominatorSum -= p->second;
                        //         velocitySamples.pop();
                        //     }

                        //     velocitySamples.push({mmDeltaD, microsDeltaT});
                        //     velocitySamplesNumeratorSum += mmDeltaD;
                        //     velocitySamplesDenominatorSum += microsDeltaT;

                        //     uint64_t sample_mm_per_s_x1000 =
                        //         (velocitySamplesNumeratorSum * 1000000) * 1000 /
                        //         velocitySamplesDenominatorSum;  // microm/micros with a *1000 for decimals

                        //     // this has alpha 1/16
                        //     uint64_t oldVelocity = velocityEstimate;
                        //     velocityEstimate = ((oldVelocity * 15) + sample_mm_per_s_x1000) >> 4;
                        // }
                        // Not Using Velocity samples ----------------------------------------
                        // microm/micros with a *1000 for decimals
                        if (newSensorsPassed >= 5) {
                            // alpha = 1/8 means k=3, so 8-1 and bit shift

                            uint64_t nextSample = (mmDeltaD * 1000000) * 1000 / microsDeltaT;

                            uint64_t lastEstimate = velocityEstimate;

                            velocityEstimate = ((lastEstimate * 15) + nextSample) >> 4;
                        } else {
                            // char debugBuff[200] = {0};
                            // printer_proprietor::formatToString(debugBuff, 200, " newSensorsPassed: %d",
                            //                                    newSensorsPassed);
                            // printer_proprietor::debug(printerProprietorTid, debugBuff);
                        }

                        //---------------------------------------------------------------------
                        printer_proprietor::updateTrainVelocity(printerProprietorTid, trainIndex, velocityEstimate);

                        prevSensorPredicitionMicros = sensorAheadMicros;
                        // what is the expected time of arrival given our current velocity?
                        sensorAheadMicros = curMicros + (distance * 1000 * 1000000 / velocityEstimate);

                        prevMicros = curMicros;
                        lastPrintTime = 0;

                        // unreserve the last sensor
                    }
                    int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
                    int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
                    int64_t estimateDistanceDiffmm = ((int64_t)velocityEstimate * estimateTimeDiffmicros) / 1000000000;

                    printer_proprietor::updateSensor(printerProprietorTid, box, sensorNum, estimateTimeDiffms,
                                                     estimateDistanceDiffmm);

                    boxBehind = box;
                    sensorNumBehind = sensorNum;

                    // need to update sensor for train as well
                    printer_proprietor::updateTrainNextSensor(printerProprietorTid, trainIndex, boxAhead,
                                                              sensorNumAhead);

                    break;
                }
                default:
                    return;
            }

        } else if (clientTid == notifierTid) {  // so we have some velocity estimate to start with

            int64_t curMicros = timerGet();
            emptyReply(clientTid);
            // how long it's been since we last hit a sensor
            uint64_t microsDeltaT = curMicros - prevMicros;

            // how long it's been since we last printed
            uint64_t printMicrosDeltaT = curMicros - lastPrintTime;
            if (!lastPrintTime) {
                printMicrosDeltaT -= prevMicros;  // for initializtion, we don't have a last print time
            }

            // how many milliseconds since last time we hit a sensor? multiply that by our velocity to get distance
            int64_t estimateDistanceTravelledmm = ((int64_t)velocityEstimate * microsDeltaT) / 1000000000;
            currentDistancePrediction = sensorAheadDistance - estimateDistanceTravelledmm;

            // we should check if our stopping distance position has gotten over a sensor since last time
            // if it is, then we make a reservation request. if it fails, issue a stop command
            if (currentDistancePrediction <= STOPPING_THRESHOLD + stoppingDistance && reservationFlag) {
                // make a reservation request for that sensor
                // should this spin up a high priority courier? Or reply to one we already have?
                char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};

                localization_server::makeReservation(parentTid, trainIndex, boxAhead, sensorNumAhead, replyBuff);

                if (replyFromByte(replyBuff[0]) == Reply::RESERVATION_FAILURE) {
                    // need to break NOW
                    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, myTrainNumber);
                    slowingDown = curMicros;
                    char debugBuff[200] = {0};
                    printer_proprietor::formatToString(debugBuff, 200,
                                                       "[Train %u] FAILED Reservation for zone %d with sensor: %c%d",
                                                       myTrainNumber, replyBuff[1], boxAhead, sensorNumAhead);
                    printer_proprietor::debug(printerProprietorTid, debugBuff);
                    // try again in a bit? after we stopped?
                } else {
                    // add our new zone we just reserved:
                    std::pair<std::pair<char, char>, std::pair<unsigned int, int>> newZoneExit;
                    newZoneExit.first.first = boxAhead;
                    newZoneExit.first.second = sensorNumAhead;
                    newZoneExit.second.first = replyBuff[1];
                    newZoneExit.second.second = REAR_OF_TRAIN - currentDistancePrediction;
                    zoneExits.push(newZoneExit);
                    zoneStatusChange = true;

                    char debugBuff[200] = {0};
                    printer_proprietor::formatToString(debugBuff, 200,
                                                       "[Train %u] Made Reservation with zone %d with sensor: %c%d",
                                                       myTrainNumber, replyBuff[1], boxAhead, sensorNumAhead);
                    printer_proprietor::debug(printerProprietorTid, debugBuff);
                }
                recentZoneAddedFlag = true;
                reservationFlag = false;  // so we aren't reserving the zone ahead of us multiple times
            }

            if (sensorAheadDistance && !slowingDown && !stopped) {
                printer_proprietor::updateTrainDistance(printerProprietorTid, trainIndex, currentDistancePrediction);
            } else if (((curMicros - slowingDown) / 1000) > 1000 * 6) {  // 6 seconds for slowing down?
                // insert acceleration model here
                slowingDown = 0;
                stopped = true;
            } else if (stopped) {
                // try and make a reservation request again. If it works, go back to your speed?
            }

            // can we free any zones?
            for (auto it = zoneExits.begin(); it != zoneExits.end(); ++it) {
                int distance = (*it).second.second;
                char reservationSensorBox = (*it).first.first;
                char reservationSensorNum = (*it).first.second;

                if (reservationSensorBox == boxAhead && reservationSensorNum == sensorNumAhead && recentZoneAddedFlag) {
                    // this should only potentially happen on the last one, if we just added a zone and don't want to
                    //  subtract our travelled distance to it on the same iteration we added it (wait until next notif)
                    recentZoneAddedFlag = false;
                    break;
                }
                // if (newSensorsPassed < 1) break;

                int64_t estimateDistanceTravelledmm = ((int64_t)velocityEstimate * printMicrosDeltaT) / 1000000000;
                distance += estimateDistanceTravelledmm;
                (*it).second.second = distance;
                // char debugBuff[200] = {0};
                // printer_proprietor::formatToString(debugBuff, 200, "[Train %u] offset from sensor %c%u is %d",
                //                                    myTrainNumber, reservationSensorBox, reservationSensorNum,
                //                                    distance);
                // printer_proprietor::debug(printerProprietorTid, debugBuff);
                if (distance >= 0) {
                    // free the reservation
                    char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
                    localization_server::freeReservation(parentTid, trainIndex, reservationSensorBox,
                                                         reservationSensorNum, replyBuff);

                    char debugBuff[200] = {0};
                    if (replyFromByte(replyBuff[0]) == Reply::FREE_SUCESS) {
                        printer_proprietor::formatToString(
                            debugBuff, 200, "[Train %u] Freed Reservation with zone: %d with sensor: %c%d",
                            myTrainNumber, replyBuff[1], reservationSensorBox, reservationSensorNum);
                        printer_proprietor::debug(printerProprietorTid, debugBuff);
                        zoneStatusChange = true;
                        zoneExits.pop();
                    } else {
                        ASSERT(0, "distance: %d sensor: %c%d", distance, reservationSensorBox, reservationSensorNum);
                        // free failed
                        printer_proprietor::formatToString(debugBuff, 200,
                                                           "[Train %u] Freed Reservation FAILED with sensor %c%d",
                                                           myTrainNumber, reservationSensorBox, reservationSensorNum);
                        printer_proprietor::debug(printerProprietorTid, debugBuff);
                    }
                }
            }

            if (zoneStatusChange) {
                ASSERT(!zoneExits.empty(), " the zone buffer was empty but we said there was a status change");
                // set up char buff
                char strBuff[200] = {0};
                int strSize = 0;
                for (auto it = zoneExits.begin(); it != zoneExits.end(); ++it) {
                    int zonenum = (*it).second.first;
                    printer_proprietor::formatToString(strBuff + strSize, 200, "%u ", zonenum);
                    strSize += strlen(strBuff);
                }
                printer_proprietor::updateTrainZone(printerProprietorTid, trainIndex, strBuff);
                zoneStatusChange = false;
            }

            lastPrintTime = curMicros;

        } else {
            ASSERT(0, "someone else sent to train task?");
        }
    }
}
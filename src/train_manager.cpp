#include "train_manager.h"

#include "command.h"
#include "config.h"
#include "localization_helper.h"
#include "localization_server_protocol.h"
#include "marklin_server_protocol.h"
#include "train_protocol.h"
#include "util.h"

// some sanity

#include "clock_server_protocol.h"
#include "command_client.h"
#include "command_server_protocol.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "localization_helper.h"
#include "pathfinding.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor_task.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "track_data.h"
#include "train_data.h"
#include "train_task.h"
#include "turnout.h"

using namespace localization_server;

void initializeTrains(Train* trains, int marklinServerTid) {
    for (int i = 0; i < Config::MAX_TRAINS; i += 1) {
        trains[i].speed = 0;
        trains[i].id = trainAddresses[i];
        trains[i].active = false;
        trains[i].sensorAhead = nullptr;
        trains[i].realSensorAhead = nullptr;
        trains[i].sensorBehind = nullptr;
        trains[i].stoppingSensor = nullptr;
        trains[i].whereToIssueStop = 0;
        trains[i].stoppingDistance = getStoppingDistSeed(i);
        trains[i].targetNode = nullptr;
        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainAddresses[i]);
    }
}

#define TRAIN_TASK_PRIORITY 20
#define SENSOR_COURIER_PRIORITY 25
TrainManager::TrainManager(int marklinServerTid, int printerProprietorTid, int clockServerTid,
                           uint32_t turnoutNotifierTid)
    : marklinServerTid(marklinServerTid),
      printerProprietorTid(printerProprietorTid),
      clockServerTid(clockServerTid),
      turnoutNotifierTid(turnoutNotifierTid) {
    // starts here
    initializeTrains(trains, marklinServerTid);
// CHANGE DEPENDING ON TRACK
#if defined(TRACKA)
    initialTurnoutConfigTrackA(turnouts);
    initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);
    init_tracka(track);
#else
    initialTurnoutConfigTrackB(turnouts);
    initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);
    init_trackb(track);
#endif
    initTrackSensorInfo(track, turnouts);  // sets every sensor's "nextSensor" according to track state
    initializeDistanceMatrix(track, distanceMatrix);

    trainReservation.initialize(track, printerProprietorTid);

    for (int i = 0; i < Config::MAX_TRAINS; i++) {
        char sendBuff[5] = {0};
        trainTasks[i] = sys::Create(TRAIN_TASK_PRIORITY, TrainTask);  // stores TID
        printer_proprietor::formatToString(sendBuff, 4, "%d", trainIndexToNum(i));
        sys::Send(trainTasks[i], sendBuff, 4, nullptr, 0);
        // also spin up courier associated with train
        sensorCouriers[i] = sys::Create(SENSOR_COURIER_PRIORITY, TrainTask);
        printer_proprietor::formatToString(sendBuff, 4, "%d", trainTasks[i]);
        sys::Send(sensorCouriers[i], sendBuff, 4, nullptr, 0);
    }

    initSwitchMap(track);

    // train13[0] = &track[56];  // d9
    // train13[0] = &track[28];  // b13
    // train13[1] = &track[43];  // c12

    // // train13[1] = &track[39];  // c8
    // train13[2] = &track[63];  // d16

    // // train13[2] = &track[69];  // e6
    // // train13[2] = &track[44];  // c13
    // train13[3] = &track[54];  // d7

    train13[0] = &track[73];  // e10
    train13[1] = &track[40];  // c9
    train13[2] = &track[20];  // b5 instead
    // train13[2] = &track[50];  // d3
    train13[3] = &track[47];  // c16
    // train13[4] = &track[2];   // a3
    // train13[5] = &track[39];  // c8
    // train13[6] = &track[44];  // c13

    // train14[0] = &track[73];  // e10
    // // train14[1] = &track[17];  // b2
    // // train14[2] = &track[50];  // d3
    // // train14[3] = &track[47];  // c16
    // train14[1] = &track[40];  // c9
    // train14[2] = &track[50];  // d3
    // train14[3] = &track[47];  // c16

    // train14[0] = &track[56];  // d9
    // train14[1] = &track[39];  // c8
    // train14[2] = &track[44];  // c13
    // train14[3] = &track[54];  // d7

    // uncomment this for testing offtrack
    //  trains[trainNumToIndex(16)].sensorAhead = &track[(('A' - 'A') * 16) + (3 - 1)];
    //  trains[trainNumToIndex(16)].active = true;
}
TrackNode* TrainManager::trainIndexToTrackNode(int trainIndex, int count) {
    return train13[count];
}

void TrainManager::setTrainSpeed(unsigned int trainSpeed, unsigned int trainNumber) {
    int trainIdx = trainNumToIndex(trainNumber);
    ASSERT(trains[trainIdx].active == true, "tried setting a speed to an inactive train");
    train_server::setTrainSpeed(trainTasks[trainIdx], trainSpeed);
    trains[trainIdx].speed = trainSpeed;
}

void TrainManager::processSensorReading(char* receiveBuffer) {
    int64_t curMicros = timerGet();

    char box = receiveBuffer[1];
    unsigned int sensorNum = 0;
    a2ui(receiveBuffer + 2, 10, &sensorNum);

#if defined(TRACKA)
    if (box == 'B' && (sensorNum == 9 || sensorNum == 10)) {
        return;
    }
#endif
    int trackNodeIdx = ((box - 'A') * 16) + (sensorNum - 1);
    TrackNode* curSensor = &track[trackNodeIdx];
    Train* curTrain = nullptr;

    // Slightly better looking version, but not as expandable in the future
    for (int i = 0; i < Config::MAX_TRAINS; i++) {
        if (trains[i].active) {
            if (trains[i].realSensorAhead == curSensor) {
                curTrain = &trains[i];
                break;  // goes to the first train who has this sensor ahead of them (might not be the closest)
            }
        }
    }

    if (curTrain == nullptr) {
        return;
    }

    // we should maybe check this before searching through the trains?
    TrackNode* prevSensor = curTrain->sensorBehind;

    if (curSensor == prevSensor) {
        return;
    }

    if (!prevSensor) {
        // prevMicros = curMicros;
        curTrain->sensorBehind = curSensor;
        curTrain->sensorAhead = curSensor->nextSensor;
        curTrain->realSensorAhead = curSensor->nextSensor;
        while (curTrain->realSensorAhead->name[0] == 'F') {
            curTrain->realSensorAhead = curTrain->realSensorAhead->nextSensor;
        }

    } else {
        // update next sensor
        curTrain->sensorAhead = curSensor->nextSensor;
        curTrain->realSensorAhead = curSensor->nextSensor;
        while (curTrain->realSensorAhead->name[0] == 'F') {
            curTrain->realSensorAhead = curTrain->realSensorAhead->nextSensor;
        }

        curTrain->sensorBehind = curSensor;
    }

    char nextBox = curSensor->nextSensor->name[0];
    unsigned int nextSensorNum = ((curSensor->nextSensor->num + 1) - (nextBox - 'A') * 16);
    if (nextBox == 'F') {
        nextSensorNum = curSensor->nextSensor->num;
    }
    // prevSensorMicros = curMicros;
    courierSendSensorInfo(sensorCouriers[trainNumToIndex(curTrain->id)], box, sensorNum, nextBox, nextSensorNum);
}

#define DONE_TURNOUTS 1
void TrainManager::processTurnoutNotifier() {
    if (turnoutQueue.empty() && stopTurnoutNotifier) {
        char reply = DONE_TURNOUTS;
        sys::Reply(turnoutNotifierTid, &reply, 1);
        stopTurnoutNotifier = false;
    } else if (!turnoutQueue.empty()) {
        char sendBuff[3];
        std::pair<char, char> c = *(turnoutQueue.pop());
        printer_proprietor::formatToString(sendBuff, 3, "%c%c", c.first, c.second);
        sys::Reply(turnoutNotifierTid, sendBuff, 3);
    }
}
void TrainManager::notifierPop(TrackNode* nextNode) {
    char sendBuff[3];
    std::pair<char, char> c = *(turnoutQueue.pop());
    printer_proprietor::formatToString(sendBuff, 3, "%c%c", c.first, c.second);
    sys::Reply(turnoutNotifierTid, sendBuff, 3);
    stopTurnoutNotifier = true;
    // instead of adding to queue, we could just make the marklin call right here, since it'll
    // only ever be one... right?
    printer_proprietor::debugPrintF(
        printerProprietorTid, "PLAYER had a branch state conflict, sent turnout notifier to switch %s", nextNode->name);
}

uint32_t TrainManager::getReverseTid() {
    return reverseTid;
}

void TrainManager::processStopping() {
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trains[stopTrainIndex].id);

    trains[stopTrainIndex].stoppingSensor = nullptr;
    trains[stopTrainIndex].stopping = false;
}

// // TODO: FIX THIS, FILLING IN INDEXES OF REPLY BUFFER WITHOUT EVEN KNOWING HOW LONG IT IS, strlen cannot fix this
// void TrainManager::processTrainRequest(char* receiveBuffer, uint32_t clientTid) {
//     char replyBuff[Config::MAX_MESSAGE_LENGTH] = {0};
//     Command command = commandFromByte(receiveBuffer[0]);
//     switch (command) {
//         case Command::INITIAL_RESERVATION: {
//             int trainIndex = (receiveBuffer[1]) - 1;
//             Train* curTrain = &trains[trainIndex];
//             TrackNode* reservationSensor = curTrain->sensorAhead;
//             int targetTrackNodeIdx = reservationSensor->id;

//             if (trainReservation.reservationAttempt(reservationSensor, trainIndexToNum(trainIndex))) {
//                 replyBuff[0] = toByte(train_server::Reply::RESERVATION_SUCCESS);

//                 uint32_t zoneNum = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
//                 char nextBox = reservationSensor->nextSensor->name[0];
//                 unsigned int nextSensorNum = ((reservationSensor->nextSensor->num + 1) - (nextBox - 'A') * 16);
//                 if (nextBox == 'F') {
//                     nextSensorNum = reservationSensor->nextSensor->num;
//                 }
//                 char box = reservationSensor->name[0];
//                 unsigned int sensorNum = ((reservationSensor->num + 1) - (box - 'A') * 16);
//                 if (box == 'F') {
//                     sensorNum = reservationSensor->num;
//                 }

//                 // so the train knows it's initialization sensor and the next one?
//                 printer_proprietor::formatToString(replyBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%c%c",
//                 zoneNum,
//                                                    nextBox, static_cast<char>(nextSensorNum), box,
//                                                    static_cast<char>(sensorNum));

//                 sys::Reply(clientTid, replyBuff, strlen(replyBuff));

//             } else {
//                 replyBuff[0] = toByte(train_server::Reply::RESERVATION_FAILURE);
//                 replyBuff[1] = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
//                 sys::Reply(clientTid, replyBuff, strlen(replyBuff));
//             }

//             return;
//             break;
//         }
//         case Command::MAKE_RESERVATION: {
//             int trainIndex = (receiveBuffer[1]) - 1;
//             Sensor reservationSensor = {.box = receiveBuffer[2], .num = static_cast<uint8_t>(receiveBuffer[3])};
//             Train* curTrain = &trains[trainIndex];

//             int targetTrackNodeIdx = trackNodeIdxFromSensor(reservationSensor);

//             ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");

//             TrackNode* curSensor = &track[targetTrackNodeIdx];

//             if (!curTrain->path.empty() && *curTrain->path.front() == curTrain->targetNode) {
//                 // fail the reservation, which should be fine
//                 // but we need to pop the last thing in the path
//                 // WE SHOULD NOT BE HITTING THIS ANYMORE

//                 TrackNode* popped = *(curTrain->path.pop());

//                 replyBuff[0] = toByte(train_server::Reply::RESERVATION_FAILURE);
//                 replyBuff[1] = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
//                 printer_proprietor::debugPrintF(printerProprietorTid,
//                                                 "Train requested a reservation while the front of their path was "
//                                                 "their target node (shouldn't happen) %s",
//                                                 curTrain->targetNode->name);
//                 sys::Reply(clientTid, replyBuff, strlen(replyBuff));

//                 return;

//             } else if (trainReservation.reservationAttempt(&track[targetTrackNodeIdx], trainIndexToNum(trainIndex)))
//             {
//                 // create a sensor so we can compare them
//                 // we don't want to enter this when reserving our destination sensor
//                 // note: if the stopping flag is working as intended, we should never be reserving this in the first
//                 // place

//                 if (!curTrain->path.empty()) {
//                     TrackNode* expectedNode = *(curTrain->path.front());
//                     Sensor expectedSensor = {.box = expectedNode->name[0],
//                                              .num = (expectedNode->num + 1) - (expectedNode->name[0] - 'A') * 16};
//                     if (expectedSensor.box == 'F') {
//                         expectedSensor.num = expectedNode->num;
//                     }

//                     if (reservationSensor == expectedSensor) {
//                         expectedNode = *(curTrain->path.pop());
//                     } else {
//                         printer_proprietor::debugPrintF(
//                             printerProprietorTid, "BAD RESERVATION reservation sensor: %c%d expected sensor: %c%d",
//                             reservationSensor.box, reservationSensor.num, expectedSensor.box, expectedSensor.num);
//                     }

//                     // if (!(reservationSensor == expectedSensor)) {
//                     //     printer_proprietor::debugPrintF(
//                     //         printerProprietorTid, "BAD RESERVATION reservation sensor: %c%d expected sensor:
//                     %c%d",
//                     //         reservationSensor.box, reservationSensor.num, expectedSensor.box, expectedSensor.num);
//                     //     // print our path
//                     //     const char* debugPath[100] = {0};
//                     //     int counter = 0;
//                     //     for (auto it = curTrain->path.begin(); it != curTrain->path.end(); ++it) {
//                     //         TrackNode* node = *it;
//                     //         debugPath[counter] = node->name;
//                     //         counter++;
//                     //     }
//                     //     char strBuff[Config::MAX_MESSAGE_LENGTH] = {0};
//                     //     int strSize = 0;
//                     //     for (int i = 0; i < counter; i++) {
//                     //         strcpy(strBuff + strSize, debugPath[i]);
//                     //         strSize += strlen(debugPath[i]);
//                     //         if (i != counter - 1) {
//                     //             strcpy(strBuff + strSize, "->");
//                     //             strSize += 2;
//                     //         }
//                     //     }
//                     //     printer_proprietor::debugPrintF(printerProprietorTid, "The path is currently: %s",
//                     strBuff);
//                     //     clock_server::Delay(clockServerTid, 100);  // flush our printer prop
//                     //     ASSERT(0, "hopefully enough time has passed for pp to flush");
//                     // }

//                     // IF YOU NEVER END UP RESERVING THE FRONT OF YOUR PATH, THE SWITCHES WILL NEVER SWITCH FOR YOU

//                     // so look at what comes after this sensor. If it's a branch, change it
//                     TrackNode* nextNode = expectedNode->edge[DIR_AHEAD].dest;

//                     // loop through our nodes until we get a branch or a sensor
//                     while (nextNode->type != NodeType::BRANCH) {
//                         if (nextNode->type == NodeType::SENSOR) break;
//                         nextNode = nextNode->edge[DIR_AHEAD].dest;
//                     }

//                     if (nextNode->type == NodeType::BRANCH) {
//                         // get the next sensor in our path after the branch
//                         TrackNode* nextExpectedSensorNode = nullptr;
//                         for (auto it = curTrain->path.begin(); it != curTrain->path.end(); ++it) {
//                             if ((*it)->type == NodeType::SENSOR) {
//                                 nextExpectedSensorNode = (*it);
//                                 break;
//                             }
//                         }
//                         ASSERT(nextExpectedSensorNode != nullptr);
//                         // figure out if the next sensor in our path differs from the current next sensor
//                         TrackNode* currentNextSensorNode = getNextSensor(curSensor, turnouts);
//                         ASSERT(currentNextSensorNode != nullptr, "there is no sensor after this branch");

//                         if (currentNextSensorNode != nextExpectedSensorNode) {
//                             // change the turnout if theres a discrepancy
//                             if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::STRAIGHT) {
//                                 // if it's straight, make it curved
//                                 turnouts[turnoutIdx(nextNode->num)].state = SwitchState::CURVED;
//                                 // add to turnout queue
//                                 turnoutQueue.push(
//                                     std::pair<char, char>(34, (nextNode->num)));  // dont want to static_cast it
//                                 if (nextNode->num == 153 || nextNode->num == 155) {
//                                     turnouts[turnoutIdx(nextNode->num + 1)].state = SwitchState::STRAIGHT;
//                                     turnoutQueue.push(
//                                         std::pair<char, char>(33, (nextNode->num + 1)));  // dont want to static_cast
//                                         it
//                                 } else if (nextNode->num == 154 || nextNode->num == 156) {
//                                     turnouts[turnoutIdx(nextNode->num - 1)].state = SwitchState::STRAIGHT;
//                                     turnoutQueue.push(
//                                         std::pair<char, char>(33, (nextNode->num - 1)));  // dont want to static_cast
//                                         it
//                                 }
//                                 // then update the impacted sensors before our branch node
//                                 setAllImpactedSensors(nextExpectedSensorNode->reverse, turnouts,
//                                 nextExpectedSensorNode,
//                                                       0);
//                             } else {
//                                 // update turnouts
//                                 turnouts[turnoutIdx(nextNode->num)].state = SwitchState::STRAIGHT;
//                                 // add to turnout queue
//                                 turnoutQueue.push(
//                                     std::pair<char, char>(33, (nextNode->num)));  // dont want to static_cast it
//                                 if (nextNode->num == 153 || nextNode->num == 155) {
//                                     turnouts[turnoutIdx(nextNode->num + 1)].state = SwitchState::CURVED;
//                                     turnoutQueue.push(std::pair<char, char>(34, (nextNode->num + 1)));
//                                 } else if (nextNode->num == 154 || nextNode->num == 156) {
//                                     turnouts[turnoutIdx(nextNode->num - 1)].state = SwitchState::CURVED;
//                                     turnoutQueue.push(std::pair<char, char>(34, (nextNode->num - 1)));
//                                 }
//                                 setAllImpactedSensors(nextExpectedSensorNode->reverse, turnouts,
//                                 nextExpectedSensorNode,
//                                                       0);
//                             }
//                             if (curTrain->realSensorAhead == currentNextSensorNode) {
//                                 curTrain->realSensorAhead = getNextRealSensor(nextNode, turnouts);
//                                 // but how should we update the train?
//                             }
//                             char sendBuff[3];
//                             std::pair<char, char> c = *(turnoutQueue.pop());
//                             printer_proprietor::formatToString(sendBuff, 3, "%c%c", c.first, c.second);
//                             sys::Reply(turnoutNotifierTid, sendBuff, 3);
//                             stopTurnoutNotifier = true;
//                             // instead of adding to queue, we could just make the marklin call right here, since
//                             it'll
//                             // only ever be one... right?
//                             printer_proprietor::debugPrintF(
//                                 printerProprietorTid, "Had a branch state conflict, sent turnout notifier to switch
//                                 %s", nextNode->name);
//                         }
//                         // printer_proprietor::debugPrintF(printerProprietorTid,
//                         //                                 "No branch conflict with the upcoming branch",
//                         //                                 nextNode->name);
//                     }
//                     // pop until the front of our path is a sensor
//                     while (!curTrain->path.empty() && !((*curTrain->path.front())->type == NodeType::SENSOR) &&
//                            reservationSensor == expectedSensor) {
//                         TrackNode* popped = *(curTrain->path.pop());
//                         // printer_proprietor::debugPrintF(printerProprietorTid, "(res) just popped node: %s",
//                         //                                 popped->name);
//                     }

//                     if (!curTrain->path.empty() && (*curTrain->path.front())->type == NodeType::SENSOR) {
//                         // printer_proprietor::debugPrintF(printerProprietorTid, "FRONT OF PATH IS %s",
//                         //                                 (*curTrain->path.front())->name);
//                     }

//                     ASSERT(!curTrain->path.empty(), "we popped too many items in our path");
//                     ASSERT((*curTrain->path.front())->type == NodeType::SENSOR,
//                            "the first thing in our path is not a sensor");
//                 } else if (curTrain == playerTrain) {
//                     // check if a branch is coming up in this reservation
//                     //  so look at what comes after this sensor. If it's a branch, change it

//                     TrackNode* nextNode = curSensor->edge[DIR_AHEAD].dest;

//                     // loop through our nodes until we get a branch or a sensor
//                     while (nextNode->type != NodeType::BRANCH) {
//                         if (nextNode->type == NodeType::SENSOR) break;
//                         nextNode = nextNode->edge[DIR_AHEAD].dest;
//                     }

//                     if (nextNode->type == NodeType::BRANCH) {
//                         // get the next sensor in our path after the branch
//                         // THIS WILL BE TRICKY WITH MIDDLE SWITCHES. It goes 154 then 153 and 156 then 155

//                         // UNLESS ITS A MIDDLE SWITCH
//                         if (playerDirection != localization_server::BranchDirection::STRAIGHT &&
//                             playerDirection != *(switchMap.get(nextNode)) && nextNode->id < 116) {  // not middle
//                             // player did a misinput
//                             playerDirection = localization_server::BranchDirection::STRAIGHT;
//                             printer_proprietor::debugPrintF(
//                                 printerProprietorTid,
//                                 "The player's turn was inconsistent with the type of switch for %s", nextNode->name);
//                         } else if (playerDirection == localization_server::BranchDirection::RIGHT &&
//                                    (nextNode->num == 154 || nextNode->num == 156)) {
//                             nextNode = nextNode->edge[DIR_STRAIGHT].dest;
//                         }

//                         // if next node is 156 and player input is [right], change to 155
//                         (nextNode->edge[DIR_STRAIGHT])
//                         // if next node is 154 and player input is [right], change to 153
//                         (nextNode->edge[DIR_STRAIGHT])

//                         switch (playerDirection) {
//                             case localization_server::BranchDirection::STRAIGHT: {
//                                 // only send a switch command if the switch was curved
//                                 if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
//                                     TrackNode* oldNextSensor = curSensor->nextSensor;
//                                     // update turnouts
//                                     turnouts[turnoutIdx(nextNode->num)].state = SwitchState::STRAIGHT;
//                                     // add to turnout queue
//                                     turnoutQueue.push(
//                                         std::pair<char, char>(33, (nextNode->num)));  // dont want to static_cast it

//                                     // this logic will default 155 and 153 straight
//                                     if (nextNode->num == 153 || nextNode->num == 155) {
//                                         turnouts[turnoutIdx(nextNode->num + 1)].state = SwitchState::CURVED;
//                                         turnoutQueue.push(std::pair<char, char>(34, (nextNode->num + 1)));
//                                     } else if (nextNode->num == 154 || nextNode->num == 156) {
//                                         turnouts[turnoutIdx(nextNode->num - 1)].state = SwitchState::CURVED;
//                                         turnoutQueue.push(std::pair<char, char>(34, (nextNode->num - 1)));
//                                     }
//                                     TrackNode* newNextSensor = getNextSensor(curSensor, turnouts);
//                                     ASSERT(newNextSensor != nullptr, "newNextSensor == nullptr");
//                                     printer_proprietor::debugPrintF(printerProprietorTid,
//                                                                     "newnextsensor for branch %s is %s",
//                                                                     nextNode->name, newNextSensor->name);
//                                     // start at the reverse of the new upcoming sensor, so we can work backwards to
//                                     // update sensors
//                                     setAllImpactedSensors(newNextSensor->reverse, turnouts, newNextSensor, 0);
//                                     // clear the user input

//                                     notifierPop(nextNode);
//                                     if (curTrain->realSensorAhead == oldNextSensor) {
//                                         curTrain->realSensorAhead = getNextRealSensor(nextNode, turnouts);
//                                     }
//                                     printer_proprietor::debugPrintF(
//                                         printerProprietorTid,
//                                         "curTrain real sensor ahead: %s sensor before branch is: %s their next sensor
//                                         " "was: %s but its now: %s", curTrain->realSensorAhead->name,
//                                         curSensor->name, curSensor->nextSensor->name,
//                                         oldNextSensor->nextSensor->name);
//                                 }

//                                 break;
//                             }
//                             case localization_server::BranchDirection::LEFT: {
//                                 // only send a switch command if the switch was curved
//                                 // also check if this tracknode is a left curved branch

//                                 if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::STRAIGHT) {
//                                     TrackNode* oldNextSensor = curSensor->nextSensor;
//                                     // if it's straight, make it curved
//                                     turnouts[turnoutIdx(nextNode->num)].state = SwitchState::CURVED;
//                                     // add to turnout queue
//                                     turnoutQueue.push(
//                                         std::pair<char, char>(34, (nextNode->num)));  // dont want to static_cast it
//                                     if (nextNode->num == 153 || nextNode->num == 155) {
//                                         turnouts[turnoutIdx(nextNode->num + 1)].state = SwitchState::STRAIGHT;
//                                         turnoutQueue.push(std::pair<char, char>(
//                                             33, (nextNode->num + 1)));  // dont want to static_cast it
//                                     } else if (nextNode->num == 154 || nextNode->num == 156) {
//                                         turnouts[turnoutIdx(nextNode->num - 1)].state = SwitchState::STRAIGHT;
//                                         turnoutQueue.push(std::pair<char, char>(
//                                             33, (nextNode->num - 1)));  // dont want to static_cast it
//                                     }
//                                     TrackNode* newNextSensor = getNextSensor(curSensor, turnouts);
//                                     ASSERT(newNextSensor != nullptr, "newNextSensor == nullptr");
//                                     // start at the reverse of the new upcoming sensor, so we can work backwards to
//                                     // update sensors
//                                     printer_proprietor::debugPrintF(printerProprietorTid,
//                                                                     "newnextsensor for branch %s is %s",
//                                                                     nextNode->name, newNextSensor->name);
//                                     setAllImpactedSensors(newNextSensor->reverse, turnouts, newNextSensor, 0);
//                                     // clear the user input
//                                     playerDirection = localization_server::BranchDirection::STRAIGHT;
//                                     notifierPop(nextNode);
//                                     if (curTrain->realSensorAhead == oldNextSensor) {
//                                         curTrain->realSensorAhead = getNextRealSensor(nextNode, turnouts);
//                                     }
//                                     printer_proprietor::debugPrintF(
//                                         printerProprietorTid,
//                                         "curTrain real sensor ahead: %s sensor before branch is: %s their next sensor
//                                         " "was: %s but its now: %s", curTrain->realSensorAhead->name,
//                                         curSensor->name, curSensor->nextSensor->name,
//                                         oldNextSensor->nextSensor->name);
//                                 }
//                                 playerDirection = localization_server::BranchDirection::STRAIGHT;
//                                 break;
//                             }
//                             case localization_server::BranchDirection::RIGHT: {
//                                 // only send a switch command if the switch was curved
//                                 // also check if this tracknode is a left curved branch
//                                 if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::STRAIGHT) {
//                                     TrackNode* oldNextSensor = curSensor->nextSensor;
//                                     // if it's straight, make it curved
//                                     turnouts[turnoutIdx(nextNode->num)].state = SwitchState::CURVED;
//                                     // add to turnout queue
//                                     turnoutQueue.push(
//                                         std::pair<char, char>(34, (nextNode->num)));  // dont want to static_cast it
//                                     if (nextNode->num == 153 || nextNode->num == 155) {
//                                         turnouts[turnoutIdx(nextNode->num + 1)].state = SwitchState::STRAIGHT;
//                                         turnoutQueue.push(std::pair<char, char>(
//                                             33, (nextNode->num + 1)));  // dont want to static_cast it
//                                     } else if (nextNode->num == 154 || nextNode->num == 156) {
//                                         turnouts[turnoutIdx(nextNode->num - 1)].state = SwitchState::STRAIGHT;
//                                         turnoutQueue.push(std::pair<char, char>(
//                                             33, (nextNode->num - 1)));  // dont want to static_cast it
//                                     }
//                                     TrackNode* newNextSensor = getNextSensor(curSensor, turnouts);
//                                     ASSERT(newNextSensor != nullptr, "newNextSensor == nullptr");
//                                     // start at the reverse of the new upcoming sensor, so we can work backwards to
//                                     // update sensors
//                                     printer_proprietor::debugPrintF(printerProprietorTid,
//                                                                     "newnextsensor for branch %s is %s",
//                                                                     nextNode->name, newNextSensor->name);
//                                     setAllImpactedSensors(newNextSensor->reverse, turnouts, newNextSensor, 0);
//                                     // clear the user input
//                                     playerDirection = localization_server::BranchDirection::STRAIGHT;
//                                     notifierPop(nextNode);
//                                     if (curTrain->realSensorAhead == oldNextSensor) {
//                                         curTrain->realSensorAhead = getNextRealSensor(nextNode, turnouts);
//                                     }
//                                     TrackNode* oldbranchSensor = getNextSensor(nextNode->reverse, turnouts);
//                                     printer_proprietor::debugPrintF(
//                                         printerProprietorTid,
//                                         "curTrain real sensor ahead: %s sensor before branch is: %s their next sensor
//                                         " "was: %s but its now: %s", curTrain->realSensorAhead->name,
//                                         curSensor->name, curSensor->nextSensor->name,
//                                         oldNextSensor->nextSensor->name);
//                                 }
//                                 playerDirection = localization_server::BranchDirection::STRAIGHT;

//                                 break;
//                             }

//                             default:
//                                 break;
//                         }
//                     }
//                 }
//                 replyBuff[0] = toByte(train_server::Reply::RESERVATION_SUCCESS);

//                 uint32_t zoneNum = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
//                 char nextBox = curSensor->nextSensor->name[0];

//                 unsigned int nextSensorNum = ((curSensor->nextSensor->num + 1) - (nextBox - 'A') * 16);
//                 if (nextBox == 'F') {
//                     nextSensorNum = curSensor->nextSensor->num;
//                 }

//                 printer_proprietor::formatToString(replyBuff + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%d",
//                 zoneNum,
//                                                    nextBox, static_cast<char>(nextSensorNum),
//                                                    curSensor->distToNextSensor);

//                 sys::Reply(clientTid, replyBuff, strlen(replyBuff));
//                 // use next box and next sensor num HERE to display
//                 // use printer proprietor call to display the next branch

//                 // some function that loops over curSensor->nextSensor until a branch
//                 TrackNode* nextBranch = curSensor->nextSensor;  // that broke here
//                 bool deadend = false;
//                 while (nextBranch->type != NodeType::BRANCH) {
//                     if (nextBranch->type == NodeType::ENTER || nextBranch->type == NodeType::EXIT) deadend = true;
//                     nextBranch = nextBranch->edge[DIR_AHEAD].dest;
//                 }
//                 if (!deadend) {
//                     // make pp call here
//                     char sbuff[Config::MAX_MESSAGE_LENGTH] = {0};
//                     if (*(switchMap.get(nextBranch)) == BranchDirection::LEFT) {
//                         printer_proprietor::formatToString(sbuff, Config::MAX_MESSAGE_LENGTH, " ╲│ %s         ",
//                                                            nextBranch->name);
//                     } else {
//                         printer_proprietor::formatToString(sbuff, Config::MAX_MESSAGE_LENGTH, "    %s │╱      ",
//                                                            nextBranch->name);
//                     }
//                     printer_proprietor::updateTrainBranch(printerProprietorTid, trainIndex + 1, sbuff);
//                 }

//                 return;
//             }  // end of "if reservation was successful"

//             // TODO
//             //  Here, on reservation failure, we should look at the TYPE of failure to see if a train is stopped in
//             the
//             //  requested reservation
//             //  if it is, we can reply to one of them with a new destination?
//             //  we should reply to one of them so THEY can send a request for a new reservation

//             replyBuff[0] = toByte(train_server::Reply::RESERVATION_FAILURE);
//             replyBuff[1] = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
//             sys::Reply(clientTid, replyBuff, strlen(replyBuff));

//             return;
//             break;
//         }
//         case Command::FREE_RESERVATION: {
//             int trainIndex = receiveBuffer[1] - 1;

//             Sensor zoneExitSensor{.box = receiveBuffer[2], .num = receiveBuffer[3]};

//             int targetTrackNodeIdx = trackNodeIdxFromSensor(zoneExitSensor);

//             ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");

//             bool result =
//                 trainReservation.freeReservation(track[targetTrackNodeIdx].reverse, trainIndexToNum(trainIndex));
//             if (result) {
//                 replyBuff[0] = toByte(train_server::Reply::FREE_SUCCESS);
//                 replyBuff[1] = trainReservation.trackNodeToZoneNum(track[targetTrackNodeIdx].reverse);
//                 //
//                 sys::Reply(clientTid, replyBuff, strlen(replyBuff));
//                 return;
//             }
//             replyBuff[0] = toByte(train_server::Reply::FREE_FAILURE);
//             sys::Reply(clientTid, replyBuff, strlen(replyBuff));
//             return;
//             break;  // just to surpress compilation warning
//         }
//         case Command::UPDATE_RESERVATION: {
//             int trainIndex = receiveBuffer[1] - 1;
//             int buffLen = strlen(receiveBuffer);
//             ReservationType rType = reservationFromByte(receiveBuffer[2]);
//             // ASSERT(0, "bufflen: %u, ReceiveBuff[bufflen - 1]: ", buffLen, receiveBuffer[buffLen - 1]);
//             for (int i = 3; i + 1 <= buffLen; i += 2) {
//                 // ASSERT(i + 1 < buffLen - 1, "bufflen: %u, ReceiveBuff[bufflen - 1]: %d ", buffLen,
//                 //        receiveBuffer[buffLen - 1]);
//                 Sensor reservedZoneSensor{.box = receiveBuffer[i], .num = receiveBuffer[i + 1]};
//                 int targetTrackNodeIdx = trackNodeIdxFromSensor(reservedZoneSensor);
//                 TrackNode* sensorNode = &track[targetTrackNodeIdx];
//                 trainReservation.updateReservation(sensorNode, trainIndexToNum(trainIndex), rType);
//             }

//             Train* curTrain = &trains[trainIndex];

//             // if (!curTrain->path.empty()) {
//             // AFTER this reply, set send a new stop location to whomever has the smaller path
//             // ASSERT(trainReservation.isSectionReserved(*curTrain->path.front()) != 0);
//             printer_proprietor::debugPrintF(printerProprietorTid, "About to enter checking the reservation status");
//             // uart_printf(CONSOLE, "HERE A");
//             if (trainReservation.reservationStatus(curTrain->sensorAhead) == ReservationType::STOPPED) {
//                 // im forcing one of yall to reverse
//                 // figure out who the other train is
//                 printer_proprietor::debugPrintF(printerProprietorTid, "Deadlock detected by train %u", curTrain->id);
//                 int otherTrainIndex = trainNumToIndex(trainReservation.isSectionReserved(curTrain->sensorAhead));
//                 // ASSERT(otherTrainIndex != -1, "should be a findable value");
//                 // ASSERT(otherTrainIndex != trainIndex, "train is failing on a reservation that should be theirs");
//                 Train* otherTrain = &trains[otherTrainIndex];

//                 generatePathWithoutZone(
//                     curTrain, otherTrain, trainReservation.trackNodeToZoneNum(curTrain->sensorAhead),
//                     trainReservation.trackNodeToZoneNum(curTrain->sensorAhead->reverse), clientTid, receiveBuffer);

//             } else {
//                 emptyReply(clientTid);
//             }
//             // }

//             return;
//             break;  // just to surpress compilation warning
//         }
//         case Command::NEW_DESTINATION: {
//             int trainIndex = receiveBuffer[1] - 1;

//             int signedOffset = 0;

//             Train* curTrain = &trains[trainIndex];

//             TrackNode* prevTarget = curTrain->targetNode;

//             // printer_proprietor::debugPrintF(printerProprietorTid, "GENERATING NEW DEST");

//             // must reply before generating path, since it will send a stop location back to client
//             // emptyReply(clientTid);

//             // if (curTrain->trackCount == 0 || curTrain->trackCount == 1 || curTrain->trackCount == 2) {
//             //     printer_proprietor::debugPrintF(printerProprietorTid, "GENERATING THE PATH FOR REAL");

//             generatePath(curTrain, trainIndexToTrackNode(trainNumToIndex(curTrain->id), curTrain->trackCount)->id,
//                          0);  // replies to client
//             curTrain->trackCount = (curTrain->trackCount + 1) % NODE_MAX;
//             // } else {
//             //     generatePath(curTrain, generateRandomTargetIdx(curTrain->sensorAhead), 0);  // replies to client÷
//             // }

//             return;
//             break;  // just to surpress compilation warning
//         }
//         default:
//             ASSERT(0, "bad command sent to train manager");
//             return;
//     }
// }

uint32_t TrainManager::getSmallestTrainTid() {
    return trainTasks[0];
}
uint32_t TrainManager::getLargestTrainTid() {
    return trainTasks[Config::MAX_TRAINS - 1];
}

TrackNode* TrainManager::getTrack() {
    return track;
}

Turnout* TrainManager::getTurnouts() {
    return turnouts;
}

// bc only the train manager knows the TIDs of each train
void TrainManager::reverseTrain(int trainIndex) {
    train_server::reverseTrain(trainTasks[trainIndex]);
}

int indexFromStopCommand(char box, unsigned int nodeNum) {
    int targetTrackNodeIdx = 0;
    if (box >= 'A' && box <= 'E') {
        targetTrackNodeIdx = ((box - 'A') * 16) + (nodeNum - 1);  // our target's index in the track node
    } else if (box == 'F') {                                      // Branch
        targetTrackNodeIdx = (2 * (nodeNum - 1)) + 80;
    } else if (box == 'G') {  // Merge
        targetTrackNodeIdx = (2 * (nodeNum - 1)) + 81;
    } else if (box == 'H') {  // Enter
        targetTrackNodeIdx = (2 * (nodeNum - 1)) + 124;
    } else if (box == 'I') {  // Exit
        targetTrackNodeIdx = (2 * (nodeNum - 1)) + 125;
    }
#ifndef TRACKA
    if (box == 'H' || box == 'I') {
        if (nodeNum > 8) targetTrackNodeIdx -= 4;
        if (nodeNum == 7) targetTrackNodeIdx -= 2;
    }
#endif
    return targetTrackNodeIdx;
}

void TrainManager::setTrainStop(char* receiveBuffer) {
    int trainNumber = static_cast<int>(receiveBuffer[1]);
    int trainIndex = trainNumToIndex(trainNumber);

    trains[trainIndex].stopping = true;
    Train* curTrain = &trains[trainIndex];
    char box = receiveBuffer[2];
    int nodeNum = static_cast<int>(receiveBuffer[3]);
    char* buff = &receiveBuffer[4];
    unsigned int offset = 0;
    int signedOffset = 0;
    bool negativeFlag = false;
    if (*buff == '-') {
        negativeFlag = true;
        buff++;
    }
    a2ui(buff, 10, &offset);
    signedOffset = offset;
    if (negativeFlag) {
        signedOffset *= -1;
    }
    int targetTrackNodeIdx = indexFromStopCommand(box, nodeNum);

    generatePath(curTrain, targetTrackNodeIdx, signedOffset);
}

uint64_t TrainManager::generateRandomTargetIdx(TrackNode* curSensor) {
    TrackNode* targetNode = nullptr;
    while (!targetNode || !targetNode->nextSensor || distanceMatrix[curSensor->id][targetNode->id] < 100 ||
           distanceMatrix[curSensor->id][targetNode->reverse->id] < 100 ||
           distanceMatrix[curSensor->reverse->id][targetNode->id] < 100 ||
           distanceMatrix[curSensor->reverse->id][targetNode->reverse->id] < 100) {
        unsigned int seed = timerGet();
        char box = 'A' + (seed % 5);    // Random letter A-E
        int nodeNum = 1 + (seed % 16);  // Random number 1-16
        int targetTrackNodeIdx = ((box - 'A') * 16) + (nodeNum - 1);
        targetNode = &track[targetTrackNodeIdx];
    }

    return targetNode->id;
}

// this will override the trains path
void TrainManager::generatePath(Train* curTrain, int targetTrackNodeIdx, int signedOffset) {
    // if signed offset is a big negative number, then we're fine
    // but if larger than our stopping distance, we need to choose a new target
    int stoppingDistance = curTrain->stoppingDistance - signedOffset;

    TrackNode* targetNode = &track[targetTrackNodeIdx];

    // iterate over nodes until the difference is larger than zero
    // "fakes" a target that is within our stopping distance
    while (stoppingDistance < 0) {
        ASSERT(0, "im gonna assume we don't hit this in testing");
        if (targetNode->type == NodeType::BRANCH) {
            if (turnouts[turnoutIdx(targetNode->num)].state == SwitchState::CURVED) {
                stoppingDistance += targetNode->edge[DIR_CURVED].dist;
                targetNode = targetNode->edge[DIR_CURVED].dest;
            } else {
                stoppingDistance += targetNode->edge[DIR_STRAIGHT].dist;
                targetNode = targetNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (targetNode->type == NodeType::EXIT) {
            // ex: sensor A2, A14, A15 don't have a next sensor
            ASSERT(0, "Offset was too large and you hit a dead end");
            break;
        } else {
            stoppingDistance += targetNode->edge[DIR_AHEAD].dist;
            targetNode = targetNode->edge[DIR_AHEAD].dest;
        }
    }

    curTrain->targetNode = targetNode;
    // printer_proprietor::debugPrintF(printerProprietorTid, "curtrain->targetNode: %s ", targetNode->name);

    curTrain->stoppingDistance = stoppingDistance;
    RingBuffer<TrackNode*, 1000> backwardsPath;
    // since we start our search with a sensor ahead, our path will always start with a sensor
    // if THIS TRAIN has already reserved the sensor ahead, use the sensor AFTER that
    // because we expect the train's next reservation to be the first sensor in this path
    TrackNode* sourceOne = curTrain->sensorAhead;
    TrackNode* sourceTwo = nullptr;
    if (curTrain->sensorBehind) {
        sourceTwo = curTrain->sensorBehind->reverse;
    }
    // this should no longer be true due to generating a path while stopped
    // UNLESS it is the first reservation on initialization, so we start at the next one
    while (trainReservation.isSectionReserved(sourceOne) == curTrain->id) {  // double check this is the train number
        printer_proprietor::debugPrintF(printerProprietorTid, "We already reserved ahead of sensor %s",
                                        sourceOne->name);
        sourceOne = sourceOne->nextSensor;
    }

    // this should no longer be true due to generating a path while stopped or on first sensor hit
    // ASSERT(trainReservation.isSectionReserved(sourceOne) != curTrain->id, "We already reserved ahead of sensor
    // %s",
    //        sourceOne->name);

    // pop anything that was already in the trains path
    while (!curTrain->path.empty()) {
        TrackNode* test = *(curTrain->path.pop());
        // printer_proprietor::debugPrintF(printerProprietorTid, "train's path should've been empty. Popped: %s ",
        //                                 test->name);
    }

    ASSERT(curTrain->path.size() == 0, "train's path buff should be empty");

    // printer_proprietor::debugPrintF(printerProprietorTid, "starting path with source: %s ", source->name);

    // printer_proprietor::debugPrintF(printerProprietorTid, "COMPUTING SHORTEST PATH");

    PATH_FINDING_RESULT result = computeShortestPath(sourceOne, sourceTwo, curTrain->targetNode, backwardsPath,
                                                     &trainReservation, printerProprietorTid);

    TrackNode* realTarget = *backwardsPath.front();
    curTrain->targetNode = realTarget;
    printer_proprietor::debugPrintF(printerProprietorTid, "FINISHED COMPUTING");

    // printer_proprietor::debugPrintF(printerProprietorTid, "SHORTEST PATH RESULT WAS %d", result);
    ASSERT(curTrain != nullptr);
    ASSERT(sourceOne != nullptr);

    bool pathReversed = false;
    if (result == PATH_FINDING_RESULT::REVERSE) {
        pathReversed = true;
        TrackNode* temp = curTrain->sensorAhead;
        curTrain->sensorAhead = curTrain->sensorBehind->reverse;
        curTrain->sensorBehind = temp;
        curTrain->realSensorAhead = curTrain->sensorAhead;
        while (curTrain->realSensorAhead->name[0] == 'F') {
            curTrain->realSensorAhead = curTrain->realSensorAhead->nextSensor;
        }
    } else if (result == PATH_FINDING_RESULT::NO_PATH) {
        printer_proprietor::debugPrintF(printerProprietorTid, "pathfinding couldn't find a path with source");
    }

    TrackNode* lastSensor = nullptr;
    uint64_t travelledDistance = 0;
    TrackNode* prev = nullptr;

    for (auto it = backwardsPath.begin(); it != backwardsPath.end(); ++it) {
        TrackNode* node = *it;
        uint64_t edgeDistance = 0;
        if (!prev) {
            prev = node;
            continue;
        }
        if (node->type == NodeType::BRANCH) {
            if (node->edge[DIR_CURVED].dest == prev) {
                edgeDistance += node->edge[DIR_CURVED].dist;
            }
            if (node->edge[DIR_STRAIGHT].dest == prev) {
                edgeDistance += node->edge[DIR_STRAIGHT].dist;
            }
        } else {
            edgeDistance += node->edge[DIR_AHEAD].dist;
        }
        if (!lastSensor) {
            travelledDistance += edgeDistance;
            if (node->type == NodeType::SENSOR && travelledDistance >= curTrain->stoppingDistance &&
                node->name[0] != 'F') {
                lastSensor = node;
            }
        }
        prev = node;
    }

    // this assumes that we are at constant velocity, but the train will use it's stopping distance when calculating
    //   how many ticks to wait
    curTrain->whereToIssueStop = travelledDistance - curTrain->stoppingDistance;
    curTrain->stoppingSensor = lastSensor;

    char stopBox = curTrain->stoppingSensor->name[0];
    unsigned int stopSensorNum = ((curTrain->stoppingSensor->num + 1) - (stopBox - 'A') * 16);

    if (stopBox == 'F') {
        ASSERT(0, "this should never be reached: trying to stop on a fake sensor");
        stopSensorNum = curTrain->stoppingSensor->num;
    }
    printer_proprietor::debugPrintF(printerProprietorTid,
                                    "stoppingSensor: %s, distance after sensor: %d, travelled distance: %u, stopping "
                                    "distance: %d ",
                                    lastSensor->name, travelledDistance - curTrain->stoppingDistance, travelledDistance,
                                    curTrain->stoppingDistance);

    char tarBox = curTrain->targetNode->name[0];
    unsigned int tarNum = ((curTrain->targetNode->num + 1) - (tarBox - 'A') * 16);
    if (tarBox == 'F') {
        tarNum = curTrain->targetNode->num;
    }
    char startBox = curTrain->sensorAhead->name[0];
    unsigned int startNum = ((curTrain->sensorAhead->num + 1) - (startBox - 'A') * 16);
    if (startBox == 'F') {
        startNum = curTrain->sensorAhead->num;
    }

    printer_proprietor::debugPrintF(printerProprietorTid, "SENDING THE STOP INFO");

    train_server::newDestinationReply(trainTasks[trainNumToIndex(curTrain->id)], stopBox, stopSensorNum, tarBox, tarNum,
                                      startBox, startNum, curTrain->whereToIssueStop, pathReversed);

    // traverse shortest path backwards to get our forwards path
    auto it = backwardsPath.end();
    do {
        it--;
        curTrain->path.push((*it));
    } while (it != backwardsPath.begin());

    // // for debug
    const char* debugPath[100] = {0};
    int counter = 0;
    for (auto it = curTrain->path.begin(); it != curTrain->path.end(); ++it) {
        TrackNode* node = *it;
        debugPath[counter] = node->name;
        counter++;
    }
    char strBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    int strSize = 0;
    for (int i = 0; i < counter; i++) {
        strcpy(strBuff + strSize, debugPath[i]);
        strSize += strlen(debugPath[i]);
        if (i != counter - 1) {
            strcpy(strBuff + strSize, "->");
            strSize += 2;
        }
    }

    printer_proprietor::debugPrintF(printerProprietorTid, "Path: %s", strBuff);
}

void TrainManager::initializeTrain(int trainIndex, Sensor initSensor) {
    Train* curTrain = &trains[trainIndex];
    curTrain->active = true;
    int sensorIndex = trackNodeIdxFromSensor(initSensor);
    curTrain->sensorAhead = &track[sensorIndex];
    curTrain->realSensorAhead = &track[sensorIndex];

    // i should try and do the initial reservation here, and then set train speed
    // but if I just loop and block, I'll be blocking sensor inputs and stuff

    train_server::initTrain(trainTasks[trainIndex], train_server::TrainType::CHASER);

    // train_server::setTrainSpeed(trainTasks[trainIndex], TRAIN_SPEED_8);
    trains[trainIndex].speed = TRAIN_SPEED_8;
    printer_proprietor::debugPrintF(printerProprietorTid, "done sending the init");
}

void TrainManager::initializePlayer(int trainIndex, Sensor initSensor) {
    Train* curTrain = &trains[trainIndex];
    playerTrain = curTrain;
    curTrain->active = true;
    int sensorIndex = trackNodeIdxFromSensor(initSensor);
    curTrain->sensorAhead = &track[sensorIndex];
    curTrain->realSensorAhead = &track[sensorIndex];
    playerTrainIndex = trainIndex;

    // send something to the train so it knows it's the player
    train_server::initTrain(trainTasks[trainIndex], train_server::TrainType::PLAYER);

    printer_proprietor::debugPrintF(printerProprietorTid, "done telling the train it's the player");
}

void TrainManager::processPlayerInput(char input) {
    ASSERT(playerTrain != nullptr, "you didn't init the player");
    switch (input) {
        case 'w': {
            setTrainSpeed(TRAIN_SPEED_8, playerTrain->id);
            break;
        }
        case 'a': {
            playerDirection = localization_server::BranchDirection::LEFT;
            break;
        }
        case 's': {
            printer_proprietor::debugPrintF(printerProprietorTid, "sending stop to the player train");
            setTrainSpeed(TRAIN_STOP, playerTrain->id);
            break;
        }
        case 'd': {
            playerDirection = localization_server::BranchDirection::RIGHT;
            break;
        }
        case 'r': {
            reverseTrain(playerTrainIndex);
            TrackNode* temp = playerTrain->sensorAhead;
            playerTrain->sensorAhead = playerTrain->sensorBehind->reverse;
            playerTrain->sensorBehind = temp;
            playerTrain->realSensorAhead = playerTrain->sensorAhead;
            while (playerTrain->realSensorAhead->name[0] == 'F') {
                playerTrain->realSensorAhead = playerTrain->realSensorAhead->nextSensor;
            }

            break;
        }

        default:
            ASSERT(0, "bad player input was given");
            break;
    }
}

void TrainManager::initSwitchMap(TrackNode* track) {
    switchMap.insert(&track[80], localization_server::BranchDirection::LEFT);  // 1
    switchMap.insert(&track[82], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[84], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[86], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[88], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[90], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[92], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[94], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[96], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[98], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[100], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[102], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[104], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[106], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[108], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[110], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[112], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[114], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[116], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[118], localization_server::BranchDirection::LEFT);
    switchMap.insert(&track[120], localization_server::BranchDirection::RIGHT);
    switchMap.insert(&track[122], localization_server::BranchDirection::LEFT);  // 156
}

// this will override the trains path
void TrainManager::generatePathWithoutZone(Train* curTrain, Train* otherTrain, uint32_t curTrainZone,
                                           uint32_t otherTrainZone, uint32_t clientTid, char* receiveBuffer) {
    RingBuffer<TrackNode*, 1000> curTrainBackwardsPath;
    RingBuffer<TrackNode*, 1000> otherTrainBackwardsPath;

    // TrackNode* curTrainSource = curTrain->sensorAhead;
    // TrackNode* otherTrainSource = otherTrain->sensorAhead;
    // ASSERT(otherTrainSource == curTrainSource->reverse);

    // pop anything that was already in the trains path (ONLY AFTER)

    // printer_proprietor::debugPrintF(printerProprietorTid, "starting path with source: %s ", source->name);

    // printer_proprietor::debugPrintF(printerProprietorTid, "COMPUTING SHORTEST PATH");

    uint64_t curTrainPathLength = computeForwardShortestPathAvoidingZone(
        curTrain->sensorBehind->reverse, curTrain->targetNode, curTrainBackwardsPath, &trainReservation,
        printerProprietorTid, curTrainZone);
    uart_printf(CONSOLE, "HERE E ");
    uint64_t otherTrainPathLength = computeForwardShortestPathAvoidingZone(
        otherTrain->sensorBehind->reverse, otherTrain->targetNode, otherTrainBackwardsPath, &trainReservation,
        printerProprietorTid, otherTrainZone);

    Train* redirectedTrain = nullptr;
    if (curTrainPathLength > otherTrainPathLength) {
        redirectedTrain = otherTrain;
        while (!redirectedTrain->path.empty()) {
            redirectedTrain->path.pop();
        }
        // traverse shortest path backwards to get our forwards path
        auto it = otherTrainBackwardsPath.end();
        do {
            it--;
            redirectedTrain->path.push((*it));
        } while (it != otherTrainBackwardsPath.begin());
    } else {
        redirectedTrain = curTrain;
        while (!redirectedTrain->path.empty()) {
            redirectedTrain->path.pop();
        }
        // traverse shortest path backwards to get our forwards path
        auto it = curTrainBackwardsPath.end();
        do {
            it--;
            redirectedTrain->path.push((*it));
        } while (it != curTrainBackwardsPath.begin());
    }
    printer_proprietor::debugPrintF(printerProprietorTid, "MY PATH SIZE IS %u", redirectedTrain->path.size());
    // uart_printf(CONSOLE, "HERE F ");
    TrackNode* realTarget = *redirectedTrain->path.back();
    redirectedTrain->targetNode = realTarget;
    // uart_printf(CONSOLE, "HERE UNO ");
    printer_proprietor::debugPrintF(printerProprietorTid, "FINISHED COMPUTING");
    ASSERT(redirectedTrain != nullptr);
    bool pathReversed = true;
    // uart_printf(CONSOLE, "HERE DOS ");
    TrackNode* temp = redirectedTrain->sensorAhead;
    ASSERT(redirectedTrain->sensorAhead != nullptr);
    ASSERT(redirectedTrain->sensorBehind != nullptr);
    uart_printf(CONSOLE, "redirected train is: %u with sensor ahead %s and behind THAT is %s", redirectedTrain->id,
                redirectedTrain->sensorAhead->name, redirectedTrain->sensorBehind->name);
    ASSERT(redirectedTrain->sensorBehind->reverse != nullptr);

    redirectedTrain->sensorAhead = redirectedTrain->sensorBehind->reverse;
    redirectedTrain->sensorBehind = temp;
    redirectedTrain->realSensorAhead = redirectedTrain->sensorAhead;
    while (redirectedTrain->realSensorAhead->name[0] == 'F') {
        redirectedTrain->realSensorAhead = redirectedTrain->realSensorAhead->nextSensor;
    }
    // uart_printf(CONSOLE, "HERE TRES");
    TrackNode* lastSensor = nullptr;
    uint64_t travelledDistance = 0;
    TrackNode* prev = nullptr;

    // // is this even needed anymore
    // for (auto it = redirectedTrain->path.begin(); it != redirectedTrain->path.end(); ++it) {
    //     TrackNode* node = *it;
    //     uint64_t edgeDistance = 0;
    //     if (!prev) {
    //         prev = node;
    //         continue;
    //     }
    //     if (node->type == NodeType::BRANCH) {
    //         if (node->edge[DIR_CURVED].dest == prev) {
    //             edgeDistance += node->edge[DIR_CURVED].dist;
    //         }
    //         if (node->edge[DIR_STRAIGHT].dest == prev) {
    //             edgeDistance += node->edge[DIR_STRAIGHT].dist;
    //         }
    //     } else {
    //         edgeDistance += node->edge[DIR_AHEAD].dist;
    //     }
    //     if (!lastSensor) {
    //         travelledDistance += edgeDistance;
    //         if (node->type == NodeType::SENSOR && travelledDistance >= curTrain->stoppingDistance &&
    //             node->name[0] != 'F') {
    //             lastSensor = node;
    //         }
    //     }
    //     prev = node;
    // }
    // uart_printf(CONSOLE, "HERE2");
    // this assumes that we are at constant velocity, but the train will use it's stopping distance when calculating
    //   how many ticks to wait
    redirectedTrain->whereToIssueStop = travelledDistance - redirectedTrain->stoppingDistance;
    // redirectedTrain->stoppingSensor = lastSensor;
    // uart_printf(CONSOLE, "HERE3");
    // char stopBox = redirectedTrain->stoppingSensor->name[0];
    // uart_printf(CONSOLE, "HERE3.5");
    // unsigned int stopSensorNum = ((redirectedTrain->stoppingSensor->num + 1) - (stopBox - 'A') * 16);
    // uart_printf(CONSOLE, "HERE4");
    // if (stopBox == 'F') {
    //     ASSERT(0, "this should never be reached: trying to stop on a fake sensor");
    //     stopSensorNum = redirectedTrain->stoppingSensor->num;
    // }

    char tarBox = redirectedTrain->targetNode->name[0];
    unsigned int tarNum = ((redirectedTrain->targetNode->num + 1) - (tarBox - 'A') * 16);
    if (tarBox == 'F') {
        tarNum = redirectedTrain->targetNode->num;
    }
    char startBox = redirectedTrain->sensorAhead->name[0];
    unsigned int startNum = ((redirectedTrain->sensorAhead->num + 1) - (startBox - 'A') * 16);
    if (startBox == 'F') {
        startNum = redirectedTrain->sensorAhead->num;
    }
    // uart_printf(CONSOLE, "HERE5");
    printer_proprietor::debugPrintF(printerProprietorTid, "SENDING THE STOP INFO");

    // train_server::newDestinationReply(trainTasks[trainNumToIndex(redirectedTrain->id)], 'A', 5, tarBox, tarNum,
    //                                   startBox, startNum, redirectedTrain->whereToIssueStop, pathReversed);

    if (curTrain == redirectedTrain) {
        train_server::newUpdateReply(trainTasks[trainNumToIndex(redirectedTrain->id)], 'A', 5, tarBox, tarNum, startBox,
                                     startNum, redirectedTrain->whereToIssueStop, pathReversed);
        ReservationType rType = ReservationType::ONGOING;
        int buffLen = strlen(receiveBuffer);
        // ASSERT(0, "bufflen: %u, ReceiveBuff[bufflen - 1]: ", buffLen, receiveBuffer[buffLen - 1]);
        for (int i = 3; i + 1 <= buffLen; i += 2) {
            // ASSERT(i + 1 < buffLen - 1, "bufflen: %u, ReceiveBuff[bufflen - 1]: %d ", buffLen,
            //        receiveBuffer[buffLen - 1]);
            Sensor reservedZoneSensor{.box = receiveBuffer[i], .num = receiveBuffer[i + 1]};
            int targetTrackNodeIdx = trackNodeIdxFromSensor(reservedZoneSensor);
            TrackNode* sensorNode = &track[targetTrackNodeIdx];
            trainReservation.updateReservation(sensorNode, curTrain->id, rType);
        }
        uart_printf(CONSOLE, " HERE AFTER REPLYING TO TRAIN ");

        // // for debug
        const char* debugPath[100] = {0};
        int counter = 0;
        for (auto it = redirectedTrain->path.begin(); it != redirectedTrain->path.end(); ++it) {
            TrackNode* node = *it;
            debugPath[counter] = node->name;
            counter++;
        }
        char strBuff[Config::MAX_MESSAGE_LENGTH] = {0};
        int strSize = 0;
        for (int i = 0; i < counter; i++) {
            strcpy(strBuff + strSize, debugPath[i]);
            strSize += strlen(debugPath[i]);
            if (i != counter - 1) {
                strcpy(strBuff + strSize, "->");
                strSize += 2;
            }
        }

        printer_proprietor::debugPrintF(printerProprietorTid, "Path: %s", strBuff);

    } else {
        emptyReply(clientTid);
    }
}
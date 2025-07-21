#include "train_manager.h"

#include "command.h"
#include "config.h"
#include "localization_helper.h"
#include "localization_server_protocol.h"
#include "marklin_server_protocol.h"
#include "train_protocol.h"
#include "util.h"

// some sanity

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
#include "train.h"
#include "turnout.h"

using namespace localization_server;

#define TRAIN_TASK_PRIORITY 20
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

    trainReservation.initialize(track, printerProprietorTid);

    for (int i = 0; i < MAX_TRAINS; i++) {
        char sendBuff[5] = {0};
        trainTasks[i] = sys::Create(TRAIN_TASK_PRIORITY, TrainTask);  // stores TID
        printer_proprietor::formatToString(sendBuff, 4, "%d", trainIndexToNum(i));
        sys::Send(trainTasks[i], sendBuff, 4, nullptr, 0);
    }

    // uncomment this for testing offtrack
    //  trains[trainNumToIndex(16)].sensorAhead = &track[(('A' - 'A') * 16) + (3 - 1)];
    //  trains[trainNumToIndex(16)].active = true;
}

void TrainManager::processInputCommand(char* receiveBuffer) {
    Command command = commandFromByte(receiveBuffer[0]);
    switch (command) {
        case Command::SET_SPEED: {
            unsigned int trainSpeed = 10 * a2d(receiveBuffer[1]) + a2d(receiveBuffer[2]);
            unsigned int trainNumber = 10 * a2d(receiveBuffer[3]) + a2d(receiveBuffer[4]);

            int trainIdx = trainNumToIndex(trainNumber);
            train_server::setTrainSpeed(trainTasks[trainIdx], trainSpeed);
            trains[trainIdx].speed = trainSpeed;
            if (!trains[trainIdx].active) {
                trains[trainIdx].active = true;
            }
            trains[trainIdx].sensorWhereSpeedChangeStarted = trains[trainIdx].sensorBehind;
            laps = 0;
            newSensorsPassed = 0;
            break;
        }
        default: {
            ASSERT(0, "bad train manager server command");
        }
    }
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
    for (int i = 0; i < MAX_TRAINS; i++) {
        if (trains[i].active) {
            // if you're active, is this your first sensor?
            // if it is, was this the same sensor hit as last time?
            if (!trains[i].sensorBehind) {
                // active but no sensor behind, hasn't hit sensor yet
                // store this guy until the end to make sure we aren't waiting on someone else
                curTrain = &trains[i];

            } else if (trains[i].sensorAhead == curSensor) {
                // if its not your first sensor, is it the sensor ahead of you?
                // NOTE: does not properly account for two trains that have the same sensor ahead of them
                curTrain = &(trains[i]);
                break;
            }
        }
    }
    // Slightly better looking version, but not as expandable in the future
    // for (int i = 0; i < MAX_TRAINS; i++) {
    //     if (trains[i].active) {
    //         curTrain = &trains[i];
    //         if (trains[i].sensorAhead == curSensor) break;
    //     }
    // }

    // for when a train has been initialized and hasn't hit a first sensor, we check to see if it isn't another train's
    // sensor that is spamming
    // char debugBuff[200] = {0};
    if (curTrain != nullptr && !curTrain->sensorBehind) {
        // go through all the trains to see if it's one of theirs right now
        for (int i = 0; i < MAX_TRAINS; i++) {
            if (trains[i].sensorBehind && trains[i].sensorBehind == curSensor) {
                // printer_proprietor::formatToString(debugBuff, 200, "[Localization] sensor spam");
                // printer_proprietor::debug(printerProprietorTid, debugBuff);
                return;
            }
        }
    }

    if (curTrain == nullptr) {
        // printer_proprietor::formatToString(
        //     debugBuff, 200, "[Localization] Could not determine a valid train for sensor hit %c%d", box, sensorNum);
        // printer_proprietor::debug(printerProprietorTid, debugBuff);
        return;
    }

    // if (!curTrain->sensorBehind) {
    //     printer_proprietor::formatToString(debugBuff, 200,
    //                                        "[Localization] [%c%u] is the FIRST sensor hit is for train [%d]", box,
    //                                        sensorNum, curTrain->id);
    //     printer_proprietor::debug(printerProprietorTid, debugBuff);
    // } else {
    //     printer_proprietor::formatToString(
    //         debugBuff, 200,
    //         "[Localization] [%c%u] is a sensor hit is for train [%d] since the sensor behind it was: %s", box,
    //         sensorNum, curTrain->id, curTrain->sensorBehind->name);
    //     printer_proprietor::debug(printerProprietorTid, debugBuff);
    // }

    // we should maybe check this before searching through the trains?
    TrackNode* prevSensor = curTrain->sensorBehind;

    if (curSensor == prevSensor) {
        return;
    }

    if (!prevSensor) {
        // prevMicros = curMicros;
        curTrain->sensorBehind = curSensor;
        curTrain->sensorAhead = curSensor->nextSensor;
        if (curSensor->nextSensor->name[0] == 'F') {
            curTrain->sensorAhead = curSensor->nextSensor->nextSensor;
        }
        curTrain->sensorWhereSpeedChangeStarted = curSensor;

    } else {
        if (curTrain->stopping && curTrain->stoppingSensor == nullptr && laps >= 1) {
            RingBuffer<TrackNode*, 1000> path;
            computeShortestPath(curTrain->sensorAhead, curTrain->targetNode, path);

            // if we need to update our turnouts, reply to the task right away
            if (!turnoutQueue.empty()) {
                char sendBuff[3];
                std::pair<char, char> c = *(turnoutQueue.pop());
                printer_proprietor::formatToString(sendBuff, 3, "%c%c", c.first, c.second);
                sys::Reply(turnoutNotifierTid, sendBuff, 3);
                stopTurnoutNotifier = true;
            }
        }
        // check if this sensor is the one a train is waiting for
        if (curTrain->stopping && curTrain->stoppingSensor == curSensor) {
            // reply to our stopping task with a calculated amount of ticks to delay
            char sendBuff[20] = {0};
            uint64_t arrivalTime = (curTrain->whereToIssueStop * 1000 * 1000000 / curTrain->velocity);
            uint16_t numOfTicks = (arrivalTime) / Config::TICK_SIZE;  // must be changed
            uIntReply(stoppingTid, numOfTicks);
            stopTrainIndex = trainNumToIndex(curTrain->id);  // assuming only one train is given the stop command
        }

        // update next sensor
        curTrain->sensorAhead = curSensor->nextSensor;
        if (curSensor->nextSensor->name[0] == 'F') {
            curTrain->sensorAhead = curSensor->nextSensor->nextSensor;
        }

        // curTrain->sensorAheadMicros = curMicros + (curSensor->distToNextSensor * 1000 * 1000000 /
        // curTrain->velocity);
        curTrain->sensorBehind = curSensor;

        // prevMicros = curMicros;
    }

    char nextBox = curSensor->nextSensor->name[0];
    unsigned int nextSensorNum = ((curSensor->nextSensor->num + 1) - (nextBox - 'A') * 16);
    if (nextBox == 'F') {
        nextSensorNum = curSensor->nextSensor->num;
    }
    // ASSERT(0, "box: %c sensorNum: %d nextBox: %c nextSensorNum: %d", box, sensorNum, nextBox, nextSensorNum);
    // ASSERT(0, "TID is: %d curtrain id: %d index is: %d", trainTasks[trainNumToIndex(curTrain->id)], curTrain->id,
    //        trainNumToIndex(curTrain->id));
    // uint64_t nextSensorDistance = curSensor->distToNextSensor;
    // if (curSensor->nextSensor->name[0] == 'F') {
    //     nextSensorDistance = curSensor->distToNextSensor + curSensor->nextSensor->distToNextSensor;
    // }
    // ASSERT(box != 'C' && sensorNum != 7, "nextBox: %c nextSensorNum: %u", nextBox, nextSensorNum);
    // printer_proprietor::debugPrintF(
    //     printerProprietorTid,
    //     "[Localization Server] Sensor Read: %c%d Current time of sensor hit: %u ms Last time: %u ms Difference: %u
    //     ms", box, sensorNum, curMicros / 1000, prevSensorMicros / 1000, (curMicros - prevSensorMicros) / 1000);
    prevSensorMicros = curMicros;
    train_server::sendSensorInfo(trainTasks[trainNumToIndex(curTrain->id)], box, sensorNum, nextBox, nextSensorNum,
                                 curSensor->distToNextSensor);
}

void TrainManager::processReverse() {
    ASSERT(!reversingTrains.empty(), "HAD A REVERSING PROCESS WITH NO REVERSING TRAINS\r\n");
    int reversingTrainIdx = *reversingTrains.pop();
    int trainSpeed = trains[reversingTrainIdx].speed;
    int trainNumber = trains[reversingTrainIdx].id;
    marklin_server::setTrainReverseAndSpeed(marklinServerTid, trainSpeed, trainNumber);
    trains[reversingTrainIdx].reversing = false;
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

uint32_t TrainManager::getReverseTid() {
    return reverseTid;
}

void TrainManager::processStopping() {
    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trains[stopTrainIndex].id);

    trains[stopTrainIndex].stoppingSensor = nullptr;
    trains[stopTrainIndex].stopping = false;
}

// TODO: FIX THIS, FILLING IN INDEXES OF REPLY BUFFER WITHOUT EVEN KNOWING HOW LONG IT IS, strlen cannot fix this
void TrainManager::processTrainRequest(char* receiveBuffer, char* replyBuffer) {
    Command command = commandFromByte(receiveBuffer[0]);
    switch (command) {
        case Command::MAKE_RESERVATION: {
            int trainIndex = (receiveBuffer[1]) - 1;

            Sensor zoneEntranceSensor{.box = receiveBuffer[2], .num = receiveBuffer[3]};

            int targetTrackNodeIdx = trackNodeIdxFromSensor(zoneEntranceSensor);

            ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");

            TrackNode* curSensor = &track[targetTrackNodeIdx];

            if (trainReservation.reservationAttempt(&track[targetTrackNodeIdx], trainIndexToNum(trainIndex))) {
                replyBuffer[0] = toByte(train_server::Reply::RESERVATION_SUCCESS);

                uint32_t zoneNum = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
                char nextBox = curSensor->nextSensor->name[0];

                unsigned int nextSensorNum = ((curSensor->nextSensor->num + 1) - (nextBox - 'A') * 16);
                if (nextBox == 'F') {
                    nextSensorNum = curSensor->nextSensor->num;
                }

                printer_proprietor::formatToString(replyBuffer + 1, Config::MAX_MESSAGE_LENGTH - 1, "%c%c%c%d", zoneNum,
                                                   nextBox, static_cast<char>(nextSensorNum),
                                                   curSensor->distToNextSensor);

                return;
            }
            replyBuffer[0] = toByte(train_server::Reply::RESERVATION_FAILURE);
            replyBuffer[1] = trainReservation.trackNodeToZoneNum(&track[targetTrackNodeIdx]);
            return;
            break;
        }
        case Command::FREE_RESERVATION: {
            int trainIndex = receiveBuffer[1] - 1;

            Sensor zoneExitSensor{.box = receiveBuffer[2], .num = receiveBuffer[3]};

            int targetTrackNodeIdx = trackNodeIdxFromSensor(zoneExitSensor);

            ASSERT(targetTrackNodeIdx != -1, "could not parse the sensor from the reservation request");

            bool result =
                trainReservation.freeReservation(track[targetTrackNodeIdx].reverse, trainIndexToNum(trainIndex));
            if (result) {
                replyBuffer[0] = toByte(train_server::Reply::FREE_SUCCESS);
                replyBuffer[1] = trainReservation.trackNodeToZoneNum(track[targetTrackNodeIdx].reverse);
                return;
            }
            replyBuffer[0] = toByte(train_server::Reply::FREE_FAILURE);
            return;
            break;  // just to surpress compilation warning
        }
        default:
            return;
    }
}

uint32_t TrainManager::getSmallestTrainTid() {
    return trainTasks[0];
}
uint32_t TrainManager::getLargestTrainTid() {
    return trainTasks[MAX_TRAINS];
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
    // if signed offset is a big negative number, then we're fine
    // but if larger than our stopping distance, we need to choose a new target
    int stoppingDistance = curTrain->stoppingDistance - signedOffset;
    TrackNode* targetNode = &track[targetTrackNodeIdx];
    // iterate over nodes until the difference is larger than zero
    // "fakes" a target that is within our stopping distance
    while (stoppingDistance < 0) {
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
    curTrain->stoppingDistance = stoppingDistance;
    curTrain->sensorWhereSpeedChangeStarted = trains[trainIndex].sensorBehind;
    RingBuffer<TrackNode*, 1000> backwardsPath;
    computeShortestPath(curTrain->sensorAhead, curTrain->targetNode, backwardsPath);
    TrackNode* lastSensor = nullptr;
    uint64_t travelledDistance = 0;
    TrackNode* prev = nullptr;
    // for debug
    const char* debugPath[100] = {0};
    int counter = 0;
    //
    for (auto it = backwardsPath.begin(); it != backwardsPath.end(); ++it) {
        TrackNode* node = *it;
        uint64_t edgeDistance = 0;
        if (!prev) {
            prev = node;
            debugPath[counter] = node->name;
            counter++;
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
            if (node->type == NodeType::SENSOR && travelledDistance >= curTrain->stoppingDistance) {
                lastSensor = node;
            }
        }
        prev = node;
        debugPath[counter] = node->name;
        counter++;
        // we also construct another ring buffer that goes from recent->end?
    }

    char strBuff[Config::MAX_MESSAGE_LENGTH] = {0};
    int strSize = 1;
    for (int i = counter - 1; i >= 0; i--) {
        strcpy(strBuff + strSize, debugPath[i]);
        strSize += strlen(debugPath[i]);
        if (i != 0) {
            strcpy(strBuff + strSize, "->");
            strSize += 2;
        }
    }
    printer_proprietor::debugPrintF(printerProprietorTid, "Path: %s", debugPath);

    // this assumes that we are at constant velocity, but the train will use it's stopping distance when calculating
    //   how many ticks to wait
    curTrain->whereToIssueStop = travelledDistance - curTrain->stoppingDistance;
    curTrain->stoppingSensor = lastSensor;

    printer_proprietor::debugPrintF(
        printerProprietorTid,
        "stoppingSensor: %s, distance after sensor: %d, travelled distance: %u, stopping distance: %d ",
        lastSensor->name, travelledDistance - curTrain->stoppingDistance, travelledDistance,
        curTrain->stoppingDistance);

    char stopBox = curTrain->stoppingSensor->name[0];
    unsigned int stopSensorNum = ((curTrain->stoppingSensor->num + 1) - (stopBox - 'A') * 16);
    if (stopBox == 'F') {
        ASSERT(0, "this should never be reached: trying to stop on a fake sensor");
        stopSensorNum = curTrain->stoppingSensor->num;
    }

    train_server::sendStopInfo(trainTasks[trainIndex], stopBox, stopSensorNum, curTrain->whereToIssueStop);

    auto it = backwardsPath.end();
    do {
        it--;
        curTrain->path.push((*it));
    } while (it != backwardsPath.begin());

    // whne a stop command is made,

    // are we assuming that a train will be at constant velocity before stopping?

    // Ideally, on a sensor hit of an active train, we can see if it has a path
    //  if it does, we can see if this sensor hit is the first element in our ring buffer
    //   if it is, then we can pop it from our path

    // when a train makes a reservation, we can check if the sensor it's using to reserve is one on it's path
    //  if it is, and the next thing in the path is a branch, we can check if it accurately reflects the turnout state
    //  if it doesn't we can send to the turnout notifier to switch that turnout

    // will there be enough time for the turnout to switch before the train goes over it?
    //  may need to increase resevervation threshold

    // should also try measuring how long it takes from getting sensor reads in train manager vs receiving them in the
    // train task

    // if train path requires reversing, then the train manager can issue a reverse command before it's stop command
    //  our routing should account for stopping at a location from both sides (unless it's an enter/exit node)
}

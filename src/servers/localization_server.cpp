#include "localization_server.h"

#include <utility>

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "command_client.h"
#include "command_server.h"
#include "command_server_protocol.h"
#include "config.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "pathfinding.h"
#include "printer_proprietor.h"
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

void setNextSensor(TrackNode* start, Turnout* turnouts) {
    int mmTotalDist = start->edge[DIR_AHEAD].dist;
    TrackNode* nextNode = start->edge[DIR_AHEAD].dest;
    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR) {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
                mmTotalDist += nextNode->edge[DIR_CURVED].dist;
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                mmTotalDist += nextNode->edge[DIR_STRAIGHT].dist;
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::EXIT) {  // pretty sure ENTER nodes do have a next sensor!
            // ex: sensor A2, A14, A15 don't have a next sensor
            deadendFlag = true;
            break;
        } else {
            mmTotalDist += nextNode->edge[DIR_AHEAD].dist;
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }

    if (!deadendFlag) {
        start->nextSensor = nextNode;
        start->distToNextSensor = mmTotalDist;
    } else {
        start->nextSensor = nullptr;
        start->distToNextSensor = 0;
    }
}

// ensure your starting node is not a sensor
TrackNode* getNextSensor(TrackNode* start, Turnout* turnouts) {
    TrackNode* nextNode = start->edge[DIR_AHEAD].dest;
    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR) {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::EXIT) {  // pretty sure ENTER nodes do have a next sensor!
            // ex: sensor A2, A14, A15 don't have a next sensor
            deadendFlag = true;
            break;
        } else {
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }
    if (!deadendFlag) {
        return nextNode;
    } else {
        return nullptr;
    }
}

#define MAX_SENSORS 80
void initTrackSensorInfo(TrackNode* track, Turnout* turnouts) {
    for (int i = 0; i < MAX_SENSORS; i++) {  // only need to look through first 80 indices (16 * 5)
        // do a dfs for the next sensor
        setNextSensor(&track[i], turnouts);
    }
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

#define DONE_TURNOUTS 1
void TurnoutNotifier() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");
    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER PROPRIETOR\r\n");

    int parentTid = sys::MyParentTid();
    for (;;) {
        char replyBuff[3];
        int res = sys::Send(parentTid, nullptr, 0, replyBuff, 3);
        handleSendResponse(res, parentTid);
        // uart_printf(CONSOLE, "turnout reply: %d %d", replyBuff[0], replyBuff[1]);
        if (replyBuff[0] != DONE_TURNOUTS) {
            marklin_server::setTurnout(marklinServerTid, replyBuff[0], replyBuff[1]);
        } else {
            marklin_server::solenoidOff(marklinServerTid);
            clock_server::Delay(clockServerTid, 20);
            continue;
        }
        clock_server::Delay(clockServerTid, 20);
        printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(replyBuff[0]),
                                          turnoutIdx(replyBuff[1]));
        // char test[20] = {0};
        // ui2a(replyBuff[1], 10, test);

        char sendBuff[100] = {0};
        // printer_proprietor::formatToString(sendBuff, 30, "set turnout: %s to straight", test)
        if (replyBuff[0] == 33) {
            printer_proprietor::formatToString(sendBuff, 100, "set turnout: %d to straight", replyBuff[1]);
        } else {
            printer_proprietor::formatToString(sendBuff, 100, "set turnout: %d to curved", replyBuff[1]);
        }
        printer_proprietor::debug(printerProprietorTid, 3, sendBuff);
    }
    sys::Exit();
}

void processInputCommand(char* receiveBuffer, Train* trains, int marklinServerTid, int printerProprietorTid,
                         int clockServerTid, RingBuffer<int, MAX_TRAINS>* reversingTrains, Turnout* turnouts,
                         TrackNode* track, uint32_t* reverseTid, RingBuffer<std::pair<char, char>, 100>* turnoutQueue,
                         int turnoutNotifierTid, bool* stopTurnoutNotifier) {
    Command command = commandFromByte(receiveBuffer[0]);
    switch (command) {
        case Command::SET_SPEED: {
            unsigned int trainSpeed = 10 * a2d(receiveBuffer[1]) + a2d(receiveBuffer[2]);
            unsigned int trainNumber = 10 * a2d(receiveBuffer[3]) + a2d(receiveBuffer[4]);

            int trainIdx = trainNumToIndex(trainNumber);

            if (!trains[trainIdx].reversing) {
                marklin_server::setTrainSpeed(marklinServerTid, trainSpeed, trainNumber);
            }
            trains[trainIdx].speed = trainSpeed;
            if (trains[trainIdx].active == false) {
                trains[trainIdx].active = true;
            }  // maybe have something that checks if the speed is zero and set the train as inactive?
            trains[trainIdx].velocity = getSeedVelocity(trainIdx, trainSpeed);
            break;
        }
        case Command::REVERSE_TRAIN: {
            int trainNumber = 10 * a2d(receiveBuffer[1]) + a2d(receiveBuffer[2]);
            int trainIdx = trainNumToIndex(trainNumber);

            if (!trains[trainIdx].reversing) {
                marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainNumber);
                reversingTrains->push(trainIdx);
                // reverse task, that notifies it's parent when done reversing
                // maybe this should be stored in the train task for multiple reversing trains
                *reverseTid = sys::Create(40, &marklin_server::reverseTrainTask);
                trains[trainIdx].reversing = true;
            }

            break;
        }
        case Command::SET_TURNOUT: {
            char turnoutDirection = receiveBuffer[1];
            char turnoutNumber = receiveBuffer[2];

            // do some processing on those sensors
            int index = turnoutIdx(turnoutNumber);
            turnouts[index].state = static_cast<SwitchState>(turnoutDirection);

            // (2 * index) + 81 will give us the merge switch index within the track array
            // MergeSwitch -> destination -> reverse will give us the impacted sensor
            TrackNode* impactedSensor = getNextSensor(&track[(2 * index) + 81], turnouts);
            setNextSensor(impactedSensor, turnouts);
            marklin_server::setTurnout(marklinServerTid, turnoutDirection, turnoutNumber);
            break;
        }
        case Command::SOLENOID_OFF: {
            marklin_server::solenoidOff(marklinServerTid);
            break;
        }
        case Command::RESET_TRACK: {
            // this can be more efficient in the future, like checking how the current switch state compares to the
            // default
#if defined(TRACKA)
            initialTurnoutConfigTrackA(turnouts);
            initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);
#else
            initialTurnoutConfigTrackB(turnouts);
            initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);
#endif
            initTrackSensorInfo(track, turnouts);
            break;
        }
        case Command::SET_STOP: {
            int trainNumber = static_cast<int>(receiveBuffer[1]);
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

            int targetTrackNodeIdx = 0;
            if (box >= 'A' && box <= 'E') {
                targetTrackNodeIdx = ((box - 'A') * 16) + (nodeNum - 1);  // our target's index in the track node
                // ASSERT(0, "reached here");
            } else if (box == 'F') {  // Branch
                targetTrackNodeIdx = (2 * (nodeNum - 1)) + 80;
            } else if (box == 'G') {  // Merge
                targetTrackNodeIdx = (2 * (nodeNum - 1)) + 81;
            } else if (box == 'H') {  // Enter
                targetTrackNodeIdx = (2 * (nodeNum - 1)) + 124;
            } else if (box == 'I') {  // Exit
                targetTrackNodeIdx = (2 * (nodeNum - 1)) + 125;
            }
            int trainIndex = trainNumToIndex(trainNumber);
            // if signed offset is a big negative number, then we're fine
            // but if larger than our stopping distance, we need to choose a new target
            int stoppingDistance = trains[trainIndex].stoppingDistance - signedOffset;
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

            RingBuffer<TrackNode*, 100> path;
            computeShortestPath(trains[trainIndex].sensorAhead, targetNode, path);
            const char* debugPath[100] = {0};
            int counter = 0;

            TrackNode* prev = nullptr;
            for (auto it = path.begin(); it != path.end(); ++it) {
                TrackNode* node = *it;
                if (prev && node->type == NodeType::BRANCH) {
                    if (node->edge[DIR_CURVED].dest == prev &&
                        turnouts[turnoutIdx(node->num)].state == SwitchState::STRAIGHT) {
                        // this complex getnextsensor should be simplified to node
                        TrackNode* impactedSensor = getNextSensor(&track[(2 * turnoutIdx(node->num)) + 81], turnouts);
                        setNextSensor(impactedSensor, turnouts);
                        // update turnouts
                        turnouts[turnoutIdx(node->num)].state = SwitchState::CURVED;
                        // add to turnout queue
                        // uart_printf(CONSOLE, "node->num: %d", node->num);
                        turnoutQueue->push(
                            std::pair<char, char>(34, static_cast<char>(node->num)));  // dont want to static_cast it
                        if (node->num == 153 || node->num == 155) {
                            turnouts[turnoutIdx(node->num + 1)].state = SwitchState::STRAIGHT;
                            turnoutQueue->push(std::pair<char, char>(
                                33, static_cast<char>(node->num + 1)));  // dont want to static_cast it
                        } else if (node->num == 154 || node->num == 156) {
                            turnouts[turnoutIdx(node->num - 1)].state = SwitchState::STRAIGHT;
                            turnoutQueue->push(std::pair<char, char>(
                                33, static_cast<char>(node->num - 1)));  // dont want to static_cast it
                        }
                    }

                    if (node->edge[DIR_STRAIGHT].dest == prev &&
                        turnouts[turnoutIdx(node->num)].state == SwitchState::CURVED) {
                        TrackNode* impactedSensor = getNextSensor(&track[(2 * turnoutIdx(node->num)) + 81], turnouts);
                        setNextSensor(impactedSensor, turnouts);
                        // update turnouts
                        turnouts[turnoutIdx(node->num)].state = SwitchState::STRAIGHT;
                        // add to turnout queue
                        // uart_printf(CONSOLE, "node->num: %d", node->num);
                        turnoutQueue->push(
                            std::pair<char, char>(33, static_cast<char>(node->num)));  // dont want to static_cast it
                        if (node->num == 153 || node->num == 155) {
                            turnouts[turnoutIdx(node->num + 1)].state = SwitchState::CURVED;
                            turnoutQueue->push(std::pair<char, char>(
                                34, static_cast<char>(node->num + 1)));  // dont want to static_cast it
                        } else if (node->num == 154 || node->num == 156) {
                            turnouts[turnoutIdx(node->num - 1)].state = SwitchState::CURVED;
                            turnoutQueue->push(std::pair<char, char>(
                                34, static_cast<char>(node->num - 1)));  // dont want to static_cast it
                        }
                    }
                }
                prev = node;
                debugPath[counter] = node->name;
                counter++;
            }

            if (!turnoutQueue->empty()) {
                char sendBuff[3];
                std::pair<char, char> c = *(turnoutQueue->pop());
                printer_proprietor::formatToString(sendBuff, 3, "%c%c", c.first, c.second);
                sys::Reply(turnoutNotifierTid, sendBuff, 3);
                *stopTurnoutNotifier = true;
            }

            char strBuff[Config::MAX_MESSAGE_LENGTH] = {0};
            int strSize = 0;

            for (int i = counter - 1; i >= 0; i--) {
                strcpy(strBuff + strSize, debugPath[i]);
                strSize += strlen(debugPath[i]);
                if (i != 0) {
                    strcpy(strBuff + strSize, "->");
                    strSize += 2;
                }
            }
            printer_proprietor::debug(printerProprietorTid, 0, strBuff);

            // ************ FIND SENSOR TO ISSUE STOP COMMAND **************

            targetNode = targetNode->reverse;

            if (targetNode->type != NodeType::SENSOR && targetNode->nextSensor == nullptr) {
                setNextSensor(targetNode, turnouts);
            }

            ASSERT(targetNode->nextSensor != nullptr, "next sensor is deadend!");
            ASSERT(stoppingDistance >= 0, "we have a negative stopping distance!");
            uint64_t travelledDistance = targetNode->distToNextSensor;

            while (travelledDistance < stoppingDistance) {
                travelledDistance += targetNode->distToNextSensor;
                targetNode = targetNode->nextSensor;
            }

            targetNode = targetNode->nextSensor->reverse;  // reorient ourselves so we know which sensor to listen for

            // the amount of distance after hitting the sensor we need to issue the stop command
            trains[trainIndex].whereToIssueStop = travelledDistance - stoppingDistance;
            trains[trainIndex].stoppingSensor = targetNode;
            char debugBuff[400] = {0};
            printer_proprietor::formatToString(
                debugBuff, 400,
                "stoppingSensor: %s, distance after sensor: %d, travelled distance: %u, stopping distance: %d",
                targetNode->name, travelledDistance - stoppingDistance, travelledDistance, stoppingDistance);
            printer_proprietor::debug(printerProprietorTid, 1, debugBuff);

            break;
        }
        default: {
            ASSERT(1 == 2, "bad localization server command");
        }
    }
}

void LocalizationServer() {
    int registerReturn = name_server::RegisterAs(LOCALIZATION_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    // could just be MyParent()
    int commandServerTid = name_server::WhoIs(COMMAND_SERVER_NAME);
    ASSERT(commandServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    Train trains[MAX_TRAINS];  // trains
    TrackNode track[TRACK_MAX];
    initializeTrains(trains, marklinServerTid);

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];
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
    initTrackSensorInfo(track, turnouts);  // ensure that this is done after turnouts are initialized

    RingBuffer<int, MAX_TRAINS> reversingTrains;  // trains

    uint32_t sensorTid = sys::Create(20, &SensorTask);
    uint32_t stoppingTid = sys::Create(49, &StopTrain);
    uint32_t client = 0;
    int res = sys::Receive(&client, nullptr, 0);
    ASSERT(client == stoppingTid, "localization startup received from someone besides the stoptrain task");

    uint32_t turnoutNotifierTid = sys::Create(49, &TurnoutNotifier);
    res = sys::Receive(&client, nullptr, 0);
    ASSERT(client == turnoutNotifierTid, "localization startup received from someone besides the turnoutNotifier task");

    uint64_t prevMicros = 0;
    int laps = 0;
    uint32_t reverseTid = 0;
    int stopTrainIndex = 0;
    uint32_t delayTicks = 0;
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;  // pair<mm(deltaD), micros(deltaT)>
    int64_t prevSensorPredicitionMicros = 0;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;

    RingBuffer<std::pair<char, char>, 100> turnoutQueue;
    bool stopTurnoutNotifier = false;

    trains[trainNumToIndex(16)].sensorAhead = &track[(('A' - 'A') * 16) + (3 - 1)];
    trains[trainNumToIndex(16)].active = true;

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[21] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, 20);
        receiveBuffer[msgLen] = '\0';

        if (clientTid == commandServerTid) {
            Command command = commandFromByte(receiveBuffer[0]);
            processInputCommand(receiveBuffer, trains, marklinServerTid, printerProprietorTid, clockServerTid,
                                &reversingTrains, turnouts, track, &reverseTid, &turnoutQueue, turnoutNotifierTid,
                                &stopTurnoutNotifier);
            emptyReply(clientTid);

        } else if (clientTid == sensorTid) {
            int64_t curMicros = timerGet();

            Train* curTrain = nullptr;
            for (int i = 0; i < MAX_TRAINS; i++) {
                if (trains[i].active) {
                    // later, will do a check to see if the sensor hit is plausible for an active train
                    curTrain = &trains[i];
                }
            }
            if (curTrain == nullptr) {
                emptyReply(clientTid);
                continue;
            }

            char box = receiveBuffer[1];
            unsigned int sensorNum = 0;
            a2ui(receiveBuffer + 2, 10, &sensorNum);

            int trackNodeIdx = ((box - 'A') * 16) + (sensorNum - 1);
            TrackNode* curSensor = &track[trackNodeIdx];
            // if this is some random sensor flick
            // need to initialize the curtrain sensor ahead or else this will error on the first sensor hit
            // if (curSensor != curTrain->sensorAhead && curSensor != curTrain->sensorAhead->nextSensor) {
            //     emptyReply(clientTid);
            //     continue;
            // }
            TrackNode* prevSensor = curTrain->sensorBehind;

            if (curSensor == prevSensor) {
                emptyReply(clientTid);
                continue;
            }

            if (!prevSensor) {
                prevMicros = curMicros;
                curTrain->sensorBehind = curSensor;
            } else {
                uint64_t microsDeltaT = curMicros - prevMicros;
                uint64_t mmDeltaD = prevSensor->distToNextSensor;

                // if ((box == 'B' && sensorNum == 1) || (box == 'B' && sensorNum == 2)) {
                //     laps++;
                // }

                // if (laps == 24) {
                //     marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, curTrain->id);
                // } else if (laps >= 14) {
                //     printer_proprietor::measurementOutput(printerProprietorTid, prevSensor->name, curSensor->name,
                //                                           microsDeltaT, mmDeltaD);
                // } else if (laps == 12) {
                //     marklin_server::setTrainSpeed(marklinServerTid, TRAIN_SPEED_8, curTrain->id);
                //     printer_proprietor::measurementOutput(printerProprietorTid, "CHANGING TO 8 HIGH", "", 0, 0);
                // } else if (laps >= 2) {
                //     printer_proprietor::measurementOutput(printerProprietorTid, prevSensor->name, curSensor->name,
                //                                           microsDeltaT, mmDeltaD);
                // }

                if (velocitySamples.full()) {
                    std::pair<uint64_t, uint64_t>* p = velocitySamples.front();
                    velocitySamplesNumeratorSum -= p->first;
                    velocitySamplesDenominatorSum -= p->second;
                    velocitySamples.pop();
                }

                velocitySamples.push({mmDeltaD, microsDeltaT});
                velocitySamplesNumeratorSum += mmDeltaD;
                velocitySamplesDenominatorSum += microsDeltaT;

                uint64_t sample_mm_per_s_x1000 =
                    (velocitySamplesNumeratorSum * 1000000) * 1000 /
                    velocitySamplesDenominatorSum;  // microm/micros with a *1000 for decimals

                // this is still alpha w 1/16
                uint64_t oldVelocity = curTrain->velocity;
                curTrain->velocity = ((oldVelocity * 15) + sample_mm_per_s_x1000) >> 4;

                // check if this sensor is the one a train is waiting for
                if (curTrain->stoppingSensor == curSensor) {
                    // reply to our stopping task with a calculated amount of ticks to delay
                    char sendBuff[20] = {0};
                    uint64_t arrivalTime = (curTrain->whereToIssueStop * 1000 * 1000000 / curTrain->velocity);
                    uint16_t numOfTicks = (arrivalTime) / Config::TICK_SIZE;  // must be changed
                    uIntReply(stoppingTid, numOfTicks);
                    stopTrainIndex =
                        trainNumToIndex(curTrain->id);  // assuming only one train is given the stop command
                }

                printer_proprietor::updateTrainStatus(printerProprietorTid, curTrain->id, curTrain->velocity);

                prevSensorPredicitionMicros = curTrain->sensorAheadMicros;

                // update next sensor
                curTrain->sensorAhead = curSensor->nextSensor;
                curTrain->sensorAheadMicros =
                    curMicros + (curSensor->distToNextSensor * 1000 * 1000000 / curTrain->velocity);
                curTrain->sensorBehind = curSensor;

                prevMicros = curMicros;
            }

            int64_t estimateTimeDiffmicros = (curMicros - prevSensorPredicitionMicros);
            int64_t estimateTimeDiffms = estimateTimeDiffmicros / 1000;
            int64_t estimateDistanceDiffmm = ((int64_t)curTrain->velocity * estimateTimeDiffmicros) / 1000000000;

            printer_proprietor::updateSensor(printerProprietorTid, box, sensorNum, estimateTimeDiffms,
                                             estimateDistanceDiffmm);

            emptyReply(clientTid);

        } else if (clientTid == reverseTid) {
            ASSERT(!reversingTrains.empty(), "HAD A REVERSING PROCESS WITH NO REVERSING TRAINS\r\n");
            int reversingTrainIdx = *reversingTrains.pop();
            int trainSpeed = trains[reversingTrainIdx].speed;
            int trainNumber = trains[reversingTrainIdx].id;
            marklin_server::setTrainReverseAndSpeed(marklinServerTid, trainSpeed, trainNumber);
            trains[reversingTrainIdx].reversing = false;
            // no need to reply, reverse task is dead
        } else if (clientTid == stoppingTid) {
            // we have delayed enough, issue the stop command:
            // ideally, we know which train this is (we are the localization server)
            // current implementation only works for one train
            marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trains[stopTrainIndex].id);
            trains[stopTrainIndex].stoppingSensor = nullptr;

        } else if (clientTid == turnoutNotifierTid) {
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

        } else {
            ASSERT(0, "Localization server received from someone unexpected. TID: %u", clientTid);
        }
    }

    sys::Exit();
}
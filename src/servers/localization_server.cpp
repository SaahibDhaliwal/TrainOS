#include "localization_server.h"

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
        } else if (nextNode->type == NodeType::ENTER || nextNode->type == NodeType::EXIT) {
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

#define MAX_SENSORS 80
void initTrackSensorInfo(TrackNode* track, Turnout* turnouts) {
    for (int i = 0; i < MAX_SENSORS; i++) {  // only need to look through first 80 indices (16 * 5)
        // do a dfs for the next sensor
        setNextSensor(&track[i], turnouts);
    }
}

void processInputCommand(char* receiveBuffer, Train* trains, int marklinServerTid, int printerProprietorTid,
                         int clockServerTid, RingBuffer<int, MAX_TRAINS>* reversingTrains, Turnout* turnouts,
                         TrackNode* track) {
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
            // if (trains[trainIdx].active == false) {
            //     trains[trainIdx].active = true;

            // }  // maybe have something that checks if the speed is zero and set the train as inactive?
            break;
        }
        case Command::REVERSE_TRAIN: {
            int trainNumber = 10 * a2d(receiveBuffer[1]) + a2d(receiveBuffer[2]);
            int trainIdx = trainNumToIndex(trainNumber);

            if (!trains[trainIdx].reversing) {
                marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainNumber);
                reversingTrains->push(trainIdx);
                // reverse task, that notifies it's parent when done reversing
                sys::Create(40, &marklin_server::reverseTrainTask);
                trains[trainIdx].reversing = true;
            }

            break;
        }
        case Command::SET_TURNOUT: {
            char turnoutDirection = receiveBuffer[1];
            char turnoutNumber = receiveBuffer[2];
            marklin_server::setTurnout(marklinServerTid, turnoutDirection, turnoutNumber);

            // do some processing on those sensors
            int index = turnoutIdx(turnoutNumber);
            turnouts[index].state = static_cast<SwitchState>(turnoutDirection);

            // (2 * index) + 81 will give us the merge switch index within the track array
            // MergeSwitch -> destination -> reverse will give us the impacted sensor
            TrackNode* impactedSensor = track[(2 * index) + 81].edge[DIR_AHEAD].dest->reverse;
            // do a dfs to assign the next sensor
            setNextSensor(impactedSensor, turnouts);
            break;
        }
        case Command::SOLENOID_OFF: {
            marklin_server::solenoidOff(marklinServerTid);
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

    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];
    initialTurnoutConfig(turnouts);
    initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);

    Train trains[MAX_TRAINS];  // trains
    initializeTrains(trains, marklinServerTid);

    RingBuffer<int, MAX_TRAINS> reversingTrains;  // trains

    uint32_t sensorTid = sys::Create(20, &SensorTask);

    TrackNode track[TRACK_MAX];
    // need to 0 this out still
    init_tracka(track);  // figure out how to tell which track it is at a later date
    initTrackSensorInfo(track, turnouts);

    uint64_t prevMicros = 0;
    // uint64_t velocity = 0;
    int laps = 0;

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[7] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, 6);
        receiveBuffer[msgLen] = '\0';

        if (clientTid == commandServerTid) {
            Command command = commandFromByte(receiveBuffer[0]);
            processInputCommand(receiveBuffer, trains, marklinServerTid, printerProprietorTid, clockServerTid,
                                &reversingTrains, turnouts, track);
            emptyReply(clientTid);

        } else if (clientTid == sensorTid) {
            // ideally, we would update the information within the train struct of which sensor is now behind and
            // upcoming need to do some simple math to get the track array index from the sensor reading thankfully, the
            // sensors are in the first section of the array so sensor B12 -> B = 16 + 12  (- 1 for array index start at
            // 0) = 28

            int64_t curMicros = timerGet();

            Train* curTrain = &trains[trainNumToIndex(14)];
            for (int i = 0; i < MAX_TRAINS; i++) {
                if (trains[i].active) {
                    // later, will do a check to see if the sensor hit is plausible for an active train
                    curTrain = &trains[i];
                }
            }

            char box = receiveBuffer[1];
            unsigned int sensorNum = 0;
            a2ui(receiveBuffer + 2, 10, &sensorNum);

            int trackNodeIdx = ((box - 'A') * 16) + (sensorNum - 1);
            TrackNode* curSensor = &track[trackNodeIdx];
            TrackNode* prevSensor = curTrain->sensorBehind;

            if (curSensor == prevSensor) {
                emptyReply(clientTid);
                continue;
            }

            uint64_t nextSample = 0;
            uint64_t lastEstimate = 0;
            int64_t prevSensorPredicitionMicros = 0;

            if (!prevSensor) {
                prevMicros = curMicros;
                curTrain->sensorBehind = curSensor;
            } else {
                if (box == 'E' && sensorNum == 13) {
                    laps++;
                }

                if (laps == 2) {
                    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_SPEED_8, 16);
                }

                if (laps == 3) {
                    marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, 16);
                }

                uint64_t microsDeltaT = curMicros - prevMicros;
                uint64_t mmDeltaD = prevSensor->distToNextSensor;

                printer_proprietor::measurementOutput(printerProprietorTid, prevSensor->name, curSensor->name,
                                                      microsDeltaT, mmDeltaD);

                uint64_t sample_mm_per_s_x1000 =
                    (mmDeltaD * 1000000) * 1000 / microsDeltaT;  // microm/micros with a *1000 for decimals

                uint64_t oldVelocity = curTrain->velocity;
                curTrain->velocity = ((oldVelocity * 7) + sample_mm_per_s_x1000 + 4) >> 3;

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

        } else {
            ASSERT(!reversingTrains.empty(), "HAD A REVERSING PROCESS WITH NO REVERSING TRAINS\r\n");
            int reversingTrainIdx = *reversingTrains.pop();
            int trainSpeed = trains[reversingTrainIdx].speed;
            int trainNumber = trains[reversingTrainIdx].id;
            marklin_server::setTrainReverseAndSpeed(marklinServerTid, trainSpeed, trainNumber);
            trains[reversingTrainIdx].reversing = false;
            // no need to reply, reverse task is dead
        }
    }

    sys::Exit();
}
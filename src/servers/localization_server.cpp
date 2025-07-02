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
#include "track_data.h"
#include "train.h"
#include "turnout.h"

using namespace localization_server;

TrackNode* getNextSensor(TrackNode start, Turnout* turnouts) {
    TrackNode* nextNode = start.edge[DIR_AHEAD].dest;
    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR) {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[nextNode->num].state == CURVED) {
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::ENTER || nextNode->type == NodeType::EXIT) {
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
        TrackNode* result = getNextSensor(track[i], turnouts);
        if (result != nullptr) track[i].nextSensor = result;
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
            impactedSensor->nextSensor = getNextSensor(*impactedSensor, turnouts);
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
    init_trackb(track);  // figure out how to tell which track it is at a later date
    initTrackSensorInfo(track, turnouts);

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
            char box = receiveBuffer[1];
            unsigned int sensorNum = 0;
            a2ui(receiveBuffer + 2, 10, &sensorNum);

            int trackNodeIdx = (('A' - box) * 16) + (sensorNum - 1);

            // trains[18].nodeBehind = &track[trackNodeIdx];
            // trains[18].nodeAhead = track[trackNodeIdx].edge->dest;

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
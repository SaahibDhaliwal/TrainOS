#include "train.h"

#include "command.h"
#include "marklin_command_protocol.h"
#include "marklin_server_protocol.h"
#include "queue.h"
#include "rpi.h"

static const int trainAddresses[MAX_TRAINS] = {14, 15, 16, 17, 18, 55};
static const int trainVelocitySeedEight[MAX_TRAINS] = {200000, 200000, 200000, 200000, 200000, 200000};
static const int trainStoppingSeed[MAX_TRAINS] = {400, 400, 400, 400, 400, 400};

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}

int getSeedVelocity(int trainIdx, int seedChoice) {
    if (seedChoice == 8) {
        return trainVelocitySeedEight[trainIdx];
    }
    return 0;
}

void initializeTrains(Train* trains, int marklinServerTid) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        trains[i].speed = 0;
        trains[i].id = trainAddresses[i];
        trains[i].reversing = false;
        trains[i].active = false;
        trains[i].velocity = trainVelocitySeedEight[i];
        trains[i].nodeAhead = nullptr;
        trains[i].nodeBehind = nullptr;
        trains[i].sensorAhead = nullptr;
        trains[i].sensorBehind = nullptr;
        trains[i].stoppingSensor = nullptr;
        trains[i].whereToIssueStop = 0;
        trains[i].stoppingDistance = trainStoppingSeed[i];
        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainAddresses[i]);
    }
}
#include "train.h"

#include "command.h"
#include "marklin_command_protocol.h"
#include "marklin_server_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "test_utils.h"

static const int trainAddresses[MAX_TRAINS] = {14, 15, 16, 17, 18, 55};
// train 55 is 10H

static const int trainFastVelocitySeedTrackB[MAX_TRAINS] = {596041, 605833, 596914, 266566, 627366, 525104};
static const int trainStopVelocitySeedTrackB[MAX_TRAINS] = {253549, 257347, 253548, 266566, 266566, 311583};
static const int trainStoppingDistSeedTrackB[MAX_TRAINS] = {400, 400, 400, 400, 400, 400};

static const int trainFastVelocitySeedTrackA[MAX_TRAINS] = {596041, 605833, 596914, 266566, 631283, 525104};
static const int trainStopVelocitySeedTrackA[MAX_TRAINS] = {253549, 257347, 253548, 266566, 267566, 311583};
static const int trainStoppingDistSeedTrackA[MAX_TRAINS] = {400, 400, 400, 400, 410, 400};

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
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
        trains[i].stoppingDistance = trainStoppingDistSeed[i];
        trains[i].stopping = false;
        trains[i].targetNode = nullptr;
        trains[i].sensorWhereStopStarted = nullptr;
        marklin_server::setTrainSpeed(marklinServerTid, TRAIN_STOP, trainAddresses[i]);
    }
}
#include "train.h"

#include "command.h"
#include "marklin_command_protocol.h"
#include "marklin_server_protocol.h"
#include "queue.h"
#include "rpi.h"

static const int trainAddresses[MAX_TRAINS] = {14, 15, 16, 17, 18, 55};

void initializeTrains(Train* trains, int marklinServerTid) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        trains[i].speed = 0;
        trains[i].id = trainAddresses[i];
        marklin_server::setTrainSpeed(marklinServerTid, Command_Byte::TRAIN_STOP, trainAddresses[i]);
    }
}

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}
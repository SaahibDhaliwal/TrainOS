#include "train.h"

#include "command.h"
#include "queue.h"

static const int trainAddresses[MAX_TRAINS] = {14, 15, 16, 17, 18, 55};

// void initialize_trains(Train* trains, Queue* marklinQueue) {
//     for (int i = 0; i < MAX_TRAINS; i += 1) {
//         Command cmd = Base_Command;
//         cmd.operation = TRAIN_STOP;
//         cmd.address = trainAddresses[i];

//         trains[i].speed = 0;
//         trains[i].reversing = false;
//         trains[i].id = trainAddresses[i];
//         queue_push(marklinQueue, cmd);
//     }
// }

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}
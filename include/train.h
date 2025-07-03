#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#include "track_data.h"

#define MAX_TRAINS 6
#define TRAIN_INIT 255

struct Train {
    bool active;
    bool reversing;
    uint8_t speed;
    uint8_t id;
    uint64_t velocity;
    // these are all ints associated with the array
    TrackNode *nodeAhead;
    TrackNode *nodeBehind;
    TrackNode *sensorAhead;
    TrackNode *sensorBehind;
};

void initializeTrains(Train *trains, int marklinServerTid);
int trainNumToIndex(int trainNum);

#endif
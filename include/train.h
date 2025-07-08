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
    int64_t sensorAheadMicros;
    TrackNode *sensorBehind;

    // stopping
    bool stopping;
    uint64_t stoppingDistance;
    TrackNode *targetNode;
    TrackNode *stoppingSensor;
    uint64_t whereToIssueStop;
    TrackNode *sensorWhereSpeedChangeStarted;
};

void initializeTrains(Train *trains, int marklinServerTid);
int trainNumToIndex(int trainNum);
int getFastVelocitySeed(int trainIdx);
int getStoppingVelocitySeed(int trainIdx);
int getStoppingDistSeed(int trainIdx);

#endif
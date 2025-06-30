#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#define MAX_TRAINS 6
#define TRAIN_INIT 255

struct Train {
    bool reversing;
    uint8_t speed;
    uint8_t id;
    uint32_t velocity;
    // these are all ints associated with the array
    uint8_t nodeAhead;
    uint8_t nodeBehind;
    uint8_t sensorAhead;
    uint8_t sensorBehind;
};

void initializeTrains(Train* trains, int marklinServerTid);
int trainNumToIndex(int trainNum);

#endif
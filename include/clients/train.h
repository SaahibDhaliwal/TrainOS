#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#define MAX_TRAINS 6

struct Train {
    uint8_t speed;
    uint8_t id;
};

void initializeTrains(Train* trains, int marklinServerTid);
int trainNumToIndex(int trainNum);

#endif
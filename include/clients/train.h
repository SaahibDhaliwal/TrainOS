#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#define MAX_TRAINS 6

typedef struct Train {
    uint8_t speed;
    bool reversing;
    uint64_t reversingStartMillis;
    uint8_t id;
} Train;

// void initialize_trains(Train* trains, Queue* merklinQueue);
int trainNumToIndex(int trainNum);

#endif
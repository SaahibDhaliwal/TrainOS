#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#include "ring_buffer.h"
#include "track_data.h"

int trainNumToIndex(int trainNum);
int trainIndexToNum(int trainIndex);
uint64_t getFastVelocitySeed(int trainIdx);
uint64_t getStoppingVelocitySeed(int trainIdx);
uint64_t getStoppingDistSeed(int trainIdx);

void TrainTask();

#endif
#ifndef __TRAIN__
#define __TRAIN__

#include <stdbool.h>
#include <stdint.h>

#include "ring_buffer.h"
#include "track_data.h"

#define MAX_TRAINS 6
#define TRAIN_INIT 255

static const int trainAddresses[MAX_TRAINS] = {13, 14, 15, 17, 18, 55};
// train 55 is 10H

// velocities are mm/s * 1000 for decimals
static const uint64_t trainFastVelocitySeedTrackB[MAX_TRAINS] = {596041, 605833, 596914, 266566, 627366, 525104};

// acceleration are mm/s^2 * 1000 for decimals
static const uint64_t trainAccelSeedTrackB[MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackB[MAX_TRAINS] = {91922, 91922, 91922, 91922, 91922, 91922};
static const uint64_t trainStopVelocitySeedTrackB[MAX_TRAINS] = {253549, 257347, 253548, 266566, 266566, 311583};
static const uint64_t trainStoppingDistSeedTrackB[MAX_TRAINS] = {370, 370, 370, 370, 370, 370};

// speed 6, loosely for tr 14
static const uint64_t trainSlowVelocitySeedTrackB[MAX_TRAINS] = {132200, 132200, 132200, 132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackB[MAX_TRAINS] = {165, 165, 165, 165, 165, 165};
static const uint64_t trainSlowVelocitySeedTrackA[MAX_TRAINS] = {132200, 132200, 132200, 132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackA[MAX_TRAINS] = {165, 165, 165, 165, 165, 165};

static const uint64_t trainFastVelocitySeedTrackA[MAX_TRAINS] = {601640, 605833, 596914, 266566, 625911, 525104};
static const uint64_t trainAccelSeedTrackA[MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackA[MAX_TRAINS] = {91922, 91922, 91922, 91922, 91922, 91922};
static const uint64_t trainStopVelocitySeedTrackA[MAX_TRAINS] = {254904, 257347, 253548, 266566, 265294, 311583};
static const uint64_t trainStoppingDistSeedTrackA[MAX_TRAINS] = {370, 370, 370, 370, 370, 370};

int trainNumToIndex(int trainNum);
int trainIndexToNum(int trainIndex);
uint64_t getFastVelocitySeed(int trainIdx);
uint64_t getStoppingVelocitySeed(int trainIdx);
uint64_t getStoppingDistSeed(int trainIdx);

void TrainTask();

#endif
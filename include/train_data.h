#define TRAIN_INIT 255
#include "config.h"
#include "cstdint"

static const int trainAddresses[Config::MAX_TRAINS] = {13, 14, 15, 17, 18, 55};

int trainNumToIndex(int trainNum);
int trainIndexToNum(int trainIndex);

uint64_t getFastVelocitySeed(int trainIdx);
uint64_t getStoppingVelocitySeed(int trainIdx);
uint64_t getSlowVelocitySeed(int trainIdx);

uint64_t getAccelerationSeed(int trainIdx);
uint64_t getDecelerationSeed(int trainIdx);

uint64_t getSlowStoppingDistSeed(int trainIdx);

uint64_t getStoppingDistSeed(int trainIdx);

int stopDistFromSpeed(int trainIndex, int speed);

uint64_t velocityFromSpeed(int trainIndex, int speed);
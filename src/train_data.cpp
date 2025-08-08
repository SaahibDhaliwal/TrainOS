#include "train_data.h"

#include "command.h"
#include "config.h"
#include "test_utils.h"

// train 55 is 10H

// velocities are mm/s * 1000 for decimals
static const uint64_t trainFastVelocitySeedTrackB[Config::MAX_TRAINS] = {596041, 605833, 596914,
                                                                         266566, 627366, 525104};

// acceleration are mm/s^2 * 1000 for decimals
static const uint64_t trainAccelSeedTrackB[Config::MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackB[Config::MAX_TRAINS] = {87922, 87922, 87922, 87922, 87922, 87922};
static const uint64_t trainStopVelocitySeedTrackB[Config::MAX_TRAINS] = {223549, 227347, 223548,
                                                                         236566, 236566, 281583};
static const uint64_t trainStoppingDistSeedTrackB[Config::MAX_TRAINS] = {370, 370, 370, 370, 370, 370};

// speed 6, loosely for tr 14
static const uint64_t trainSlowVelocitySeedTrackB[Config::MAX_TRAINS] = {132200, 132200, 132200,
                                                                         132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackB[Config::MAX_TRAINS] = {165, 165, 165, 165, 165, 165};
static const uint64_t trainSlowVelocitySeedTrackA[Config::MAX_TRAINS] = {132200, 132200, 132200,
                                                                         132200, 132200, 132200};
static const uint64_t trainSlowStoppingDistSeedTrackA[Config::MAX_TRAINS] = {165, 165, 165, 165, 165, 165};

static const uint64_t trainFastVelocitySeedTrackA[Config::MAX_TRAINS] = {601640, 605833, 596914,
                                                                         266566, 625911, 525104};
static const uint64_t trainAccelSeedTrackA[Config::MAX_TRAINS] = {78922, 78922, 78922, 78922, 78922, 78922};
static const uint64_t trainDecelSeedTrackA[Config::MAX_TRAINS] = {87922, 87922, 87922, 87922, 87922, 87922};
static const uint64_t trainStopVelocitySeedTrackA[Config::MAX_TRAINS] = {254904, 257347, 253548,
                                                                         266566, 265294, 311583};
static const uint64_t trainStoppingDistSeedTrackA[Config::MAX_TRAINS] = {370, 370, 370, 370, 370, 370};

int trainNumToIndex(int trainNum) {
    for (int i = 0; i < Config::MAX_TRAINS; i += 1) {
        if (trainAddresses[i] == trainNum) return i;
    }

    return -1;
}

int trainIndexToNum(int trainIndex) {
    return trainAddresses[trainIndex];
}

uint64_t getFastVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainFastVelocitySeedTrackA[trainIdx];
#else
    return trainFastVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getStoppingVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainStopVelocitySeedTrackA[trainIdx];
#else
    return trainStopVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getAccelerationSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainAccelSeedTrackB[trainIdx];
#else
    return trainAccelSeedTrackB[trainIdx];
#endif
}

uint64_t getDecelerationSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainDecelSeedTrackA[trainIdx];
#else
    return trainDecelSeedTrackB[trainIdx];
#endif
}

uint64_t getSlowVelocitySeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainSlowVelocitySeedTrackA[trainIdx];
#else
    return trainSlowVelocitySeedTrackB[trainIdx];
#endif
}

uint64_t getSlowStoppingDistSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainSlowStoppingDistSeedTrackA[trainIdx];
#else
    return trainSlowStoppingDistSeedTrackB[trainIdx];
#endif
}

uint64_t getStoppingDistSeed(int trainIdx) {
    ASSERT(trainIdx >= 0 && trainIdx < Config::MAX_TRAINS);

#if defined(TRACKA)
    return trainStoppingDistSeedTrackA[trainIdx];
#else
    return trainStoppingDistSeedTrackB[trainIdx];
#endif
}

int stopDistFromSpeed(int trainIndex, int speed) {
    switch (speed) {
        case TRAIN_SPEED_14:
            return getStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_8:
            return getStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_6:
            return getSlowStoppingDistSeed(trainIndex);  // speed 14 does not have a stopping dist
        default:
            return TRAIN_STOP;
    }
}

// insert acceleration model here...
uint64_t velocityFromSpeed(int trainIndex, int speed) {
    switch (speed) {
        case TRAIN_SPEED_14:
            return getFastVelocitySeed(trainIndex);
        case TRAIN_SPEED_8:
            return getStoppingVelocitySeed(trainIndex);  // speed 14 does not have a stopping dist
        case TRAIN_SPEED_6:
            return getSlowVelocitySeed(trainIndex);  // speed 14 does not have a stopping dist
        default:
            return TRAIN_STOP;
    }
}

#ifndef __TRAIN_PROTOCOL__
#define __TRAIN_PROTOCOL__
#include <cstdint>

#include "sensor.h"

namespace train_server {
enum class Command : char {
    NEW_SENSOR = 3,
    // SKIPPED_SENSOR,
    SET_SPEED,
    REVERSE,
    GET_REVERSE_TIME,
    FINISH_REVERSE,
    STOP_SENSOR,
    // LOCATION_QUERY,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char {
    SUCCESS = 1,
    RESERVATION_SUCCESS,
    RESERVATION_FAILURE,
    FREE_SUCCESS,
    FREE_FAILURE,
    INVALID_SERVER,
    COUNT,
    UNKNOWN_REPLY
};

enum class Seed : char { SEED_8H, SEED14, COUNT, UNKNOWN_COMMAND };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void sendSensorInfo(int tid, char currentBox, unsigned int currentSensorNum, char nextBox, unsigned int nextSensorNum,
                    uint64_t distance);
void setTrainSpeed(int tid, unsigned int trainSpeed);

void reverseTrain(int tid);

void finishReverse(int tid);

unsigned int getReverseDelayTicks(int tid);

void sendStopInfo(int tid, char currentBox, unsigned int currentSensorNum, uint64_t offset);

}  // namespace train_server

#endif
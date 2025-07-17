#ifndef __TRAIN_PROTOCOL__
#define __TRAIN_PROTOCOL__
#include <cstdint>

namespace train_server {
enum class Command : char {
    NEXT_SENSOR = 3,
    // SKIPPED_SENSOR,
    SET_SPEED,
    // RESERVATION_RESULT,
    // LOCATION_QUERY,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char {
    SUCCESS = 1,
    RESERVATION_SUCESS,
    RESERVATION_FAILURE,
    FREE_SUCESS,
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

}  // namespace train_server

#endif
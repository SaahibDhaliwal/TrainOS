#ifndef __LOCALIZATION_SERVER_PROTOCOL__
#define __LOCALIZATION_SERVER_PROTOCOL__

#include <cstdint>

// #include "command.h"
// #include "command_server_protocol.h"
constexpr const char* LOCALIZATION_SERVER_NAME = "localization_server";

namespace localization_server {
enum class Command : char {
    SET_SPEED = 3,
    REVERSE_TRAIN,
    SENSOR_UPDATE,
    SET_TURNOUT,
    SOLENOID_OFF,
    SET_STOP,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void setTrainSpeed(int tid, unsigned int trainSpeed, unsigned int trainNumber);
void reverseTrain(int tid, unsigned int trainNumber);

void updateSensor(int tid, char box, unsigned int sensorNum);

void setTurnout(int tid, unsigned int turnoutDirection, unsigned int turnoutNumber);
void solenoidOff(int tid);

void setStopLocation(int localizationTid, int trainNumber, char box, int sensorNum, int offset);

}  // namespace localization_server

#endif
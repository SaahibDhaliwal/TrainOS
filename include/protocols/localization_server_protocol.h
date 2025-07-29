#ifndef __LOCALIZATION_SERVER_PROTOCOL__
#define __LOCALIZATION_SERVER_PROTOCOL__

#include <cstdint>

#include "ring_buffer.h"
#include "sensor.h"
#include "zone.h"

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
    RESET_TRACK,
    INITIAL_RESERVATION,
    MAKE_RESERVATION,
    FREE_RESERVATION,
    UPDATE_RESERVATION,
    NEW_DESTINATION,
    INIT_TRAIN,
    INIT_PLAYER,
    FAKE_SENSOR_HIT,
    PLAYER_INPUT,
    COUNT,
    UNKNOWN_COMMAND
};
enum class Reply : char { SUCCESS, INVALID_SERVER, COUNT, UNKNOWN_REPLY };

enum class BranchDirection : char { LEFT = 1, RIGHT, STRAIGHT, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void setTrainSpeed(int tid, unsigned int trainSpeed, unsigned int trainNumber);
void reverseTrain(int tid, unsigned int trainNumber);

void updateSensor(int tid, char box, unsigned int sensorNum);

void setTurnout(int tid, unsigned int turnoutDirection, unsigned int turnoutNumber);
void solenoidOff(int tid);
void resetTrack(int tid);

void setStopLocation(int localizationTid, int trainNumber, char box, int sensorNum, int offset);

void makeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff);
void initReservation(int tid, int trainIndex, char* replyBuff);
void freeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff);
void updateReservation(int tid, int trainIndex, RingBuffer<ReservedZone, 32> reservedZones,
                       ReservationType reservation);
void newDestination(int tid, int trainIndex);

void initTrain(int tid, int trainIndex, Sensor initSensor);
void initPlayer(int tid, int trainIndex, Sensor initSensor);

void hitFakeSensor(int tid, int trainIndex);

void playerInput(int tid, char input);

}  // namespace localization_server

#endif
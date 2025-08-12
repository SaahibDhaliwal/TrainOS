#ifndef __RESERVATION_SERVER_PROTOCOL__
#define __RESERVATION_SERVER_PROTOCOL__

#include <cstdint>

#include "ring_buffer.h"
#include "sensor.h"
#include "zone.h"

// #include "command_server_protocol.h"
constexpr const char* RESERVATION_SERVER_NAME = "reservation_server";

namespace reservation_server {

enum class Command : char { MAKE_RESERVATION = 1, FREE_RESERVATION, UPDATE_RESERVATION, COUNT, UNKNOWN_COMMAND };
enum class Reply : char {
    RESERVATION_SUCCESS = 6,
    RESERVATION_FAIL,
    STOPPED_TRAIN_IN_ZONE,
    FREE_SUCCESS,
    FREE_FAIL,
    COUNT,
    UNKNOWN_REPLY
};

char toByte(Command c);
char toByte(Reply r);

Command commandFromByte(char c);
Reply replyFromByte(char c);

void makeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff);
void freeReservation(int tid, int trainIndex, Sensor sensor, char* replyBuff);
void updateReservation(int tid, int trainIndex, char* sendBuff, char* replyBuff);

void ReservationCourier();
void courierMakeReservation(int tid, Sensor sensor);
void courierFreeReservation(int tid, Sensor sensor);
void courierUpdateReservation(int tid, RingBuffer<ReservedZone, 32> reservedZones, ReservationType reservation);

}  // namespace reservation_server
#endif
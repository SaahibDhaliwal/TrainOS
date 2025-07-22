#ifndef __ZONE__
#define __ZONE__

// #include "localization_server_protocol.h"
#include "sensor.h"
#include "train.h"
#include "turnout.h"
#include "unordered_map.h"

#define ZONE_COUNT 34

enum class ReservationType : char { ONGOING = 3, STOPPED, PANIC, COUNT, UNKNOWN_TYPE };
char toByte(ReservationType r);
ReservationType reservationFromByte(char c);
struct ZoneSegment {
    uint32_t zoneNum;
    // if it's reserved, which train is in there?
    bool reserved = false;
    unsigned int trainNumber = 0;

    // to determine whether a train is in the zone but ahead, so safe to enter slowly
    ZoneSegment* previousZone = nullptr;
    ReservationType reservationType = ReservationType::ONGOING;
    // TrackNode* branch;
};

struct ZoneExit {
    Sensor sensorMarkingExit;
    uint64_t distanceToExitSensor;  // mm
};

struct ReservedZone {
    Sensor sensorMarkingEntrance;
    uint8_t zoneNum;
};

class TrainReservation {
   private:
    uint32_t printerProprietorTid;
    ZoneSegment zoneArray[ZONE_COUNT + 1];
    UnorderedMap<TrackNode*, ZoneSegment*, TRACK_MAX> zoneMap;

    void initZoneA(TrackNode* track);
    void initZoneB(TrackNode* track);

    ZoneSegment* mapchecker(TrackNode* sensor);

   public:
    uint32_t trackNodeToZoneNum(TrackNode* track);
    void initialize(TrackNode* track, uint32_t printerProprietorTid);

    // returns the trainNumber of the reservation if section is reserved, zero if not
    int isSectionReserved(TrackNode* start);
    bool reservationAttempt(TrackNode* entrySensor, unsigned int trainNumber);
    bool freeReservation(TrackNode* sensor, unsigned int trainNumber);
    void updateReservation(TrackNode* sensor, unsigned int trainNumber, ReservationType reservation);
    // void reserveSection(TrackNode* whereverThisNodeIs, unsigned int trainNumber);
};

#endif
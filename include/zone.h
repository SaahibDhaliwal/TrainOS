#ifndef __ZONE__
#define __ZONE__

#include "train.h"
#include "turnout.h"
#include "unordered_map.h"

#define ZONE_COUNT 34

struct ZoneSegment {
    uint32_t zoneNum;
    // if it's reserved, which train is in there?
    bool reserved = false;
    unsigned int trainNumber = 0;

    // to determine whether a train is in the zone but ahead, so safe to enter slowly
    ZoneSegment* previousZone = nullptr;
    // TrackNode* branch;
};

class TrainReservation {
   private:
    uint32_t printerProprietorTid;
    ZoneSegment zoneArray[ZONE_COUNT];
    UnorderedMap<TrackNode*, ZoneSegment*, TRACK_MAX> zoneMap;

    void initZoneA(TrackNode* track);
    void initZoneB(TrackNode* track);

    ZoneSegment* mapchecker(TrackNode* sensor);

   public:
    uint32_t trackNodeToZoneNum(TrackNode* track);
    void initialize(TrackNode* track, uint32_t printerProprietorTid);
    bool isSectionReserved(TrackNode* start);
    bool reservationAttempt(TrackNode* entrySensor, unsigned int trainNumber);
    bool freeReservation(TrackNode* sensor, unsigned int trainNumber);
    // void reserveSection(TrackNode* whereverThisNodeIs, unsigned int trainNumber);
};

#endif
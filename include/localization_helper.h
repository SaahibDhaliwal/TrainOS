#ifndef __LOCALIZATION_HELPER__
#define __LOCALIZATION_HELPER__

#include "train.h"
#include "turnout.h"

class TrainReservation {
   public:
    bool isSectionReserved();
    void reserveSection();
};

void setAllImpactedSensors(TrackNode* start, Turnout* turnouts, TrackNode* newNextSensor, int distance);
TrackNode* getNextSensor(TrackNode* start, Turnout* turnouts);
void initTrackSensorInfo(TrackNode* track, Turnout* turnouts);

#endif
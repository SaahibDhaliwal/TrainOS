#ifndef __LOCALIZATION_HELPER__
#define __LOCALIZATION_HELPER__

#include "train_task.h"
#include "turnout.h"

void setAllImpactedSensors(TrackNode* start, Turnout* turnouts, TrackNode* newNextSensor, int distance);
TrackNode* getNextSensor(TrackNode* start, Turnout* turnouts);
void initTrackSensorInfo(TrackNode* track, Turnout* turnouts);
TrackNode* getNextRealSensor(TrackNode* start, Turnout* turnouts);

#endif
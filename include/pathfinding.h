#ifndef __PATHFINDING__
#define __PATHFINDING__

#include "ring_buffer.h"
#include "track_data.h"
#include "zone.h"

enum class PATH_FINDING_RESULT { FORWARD, REVERSE, NO_PATH };

PATH_FINDING_RESULT computeShortestPath(TrackNode* sourceOne, TrackNode* sourceTwo, TrackNode* target,
                                        RingBuffer<TrackNode*, 1000>& path, TrainReservation* trainReservation,
                                        int printerTid);
void initializeDistanceMatrix(TrackNode* track, uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX]);
#endif
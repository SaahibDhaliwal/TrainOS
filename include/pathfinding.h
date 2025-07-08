#ifndef __PATHFINDING__
#define __PATHFINDING__

#include "ring_buffer.h"
#include "track_data.h"

void computeShortestPath(TrackNode* source, TrackNode* target, RingBuffer<TrackNode*, 1000>& path);

#endif
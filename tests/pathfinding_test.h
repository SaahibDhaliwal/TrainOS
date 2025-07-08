#include "pathfinding.h"
#include "ring_buffer.h"

void dijkstraTest() {
    TrackNode track[TRACK_MAX];
    init_trackb(track);

    TrackNode* B15 = &track[30];
    TrackNode* E7 = &track[70];

    RingBuffer<TrackNode*, 1000> path;

    computeShortestPath(B15, E7, path);

    TrackNode* node = nullptr;
    ASSERT(path.size() == 6);

    node = *path.front();
    ASSERT(node->name == "E7");
    path.pop();

    node = *path.front();
    ASSERT(node->name == "C13");
    path.pop();

    node = *path.front();
    ASSERT(node->name == "MR11");
    path.pop();

    node = *path.front();
    ASSERT(node->name == "BR14");
    path.pop();

    node = *path.front();
    ASSERT(node->name == "A3");
    path.pop();

    node = *path.front();
    ASSERT(node->name == "B15");
    path.pop();

    ASSERT(path.empty());
}

void runPathfindingTest() {
    // dijkstraTest2();
}

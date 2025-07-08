#include "pathfinding.h"
#include "ring_buffer.h"

void dijkstraTest2() {
    TrackNode track[TRACK_MAX];
    init_trackb(track);

    TrackNode* B2 = &track[17];
    TrackNode* EX7 = &track[135];

    RingBuffer<TrackNode*, 1000> path;

    for (int i = 0; i < 10; i++) {
        uart_printf(CONSOLE, "\n\r");
    }

    computeShortestPath(B2, EX7, path);

    for (auto it = path.begin(); it != path.end(); ++it) {
        TrackNode* node = *it;
        uart_printf(CONSOLE, "%s->", node->name);
    }

    TrackNode* node = nullptr;
    // ASSERT(path.size() == 6);

    // node = *path.front();
    // ASSERT(node->name == "E7");
    // path.pop();

    // node = *path.front();
    // ASSERT(node->name == "C13");
    // path.pop();

    // node = *path.front();
    // ASSERT(node->name == "MR11");
    // path.pop();

    // node = *path.front();
    // ASSERT(node->name == "BR14");
    // path.pop();

    // node = *path.front();
    // ASSERT(node->name == "A3");
    // path.pop();

    // node = *path.front();
    // ASSERT(node->name == "B15");
    // path.pop();

    // ASSERT(path.empty());
}

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
    dijkstraTest2();
}

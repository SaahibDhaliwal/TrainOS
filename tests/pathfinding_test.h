#include "pathfinding.h"
#include "ring_buffer.h"

void distanceMatrixTest() {
    TrackNode track[TRACK_MAX];
    init_trackb(track);

    uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX];

    initializeDistanceMatrix(track, distanceMatrix);
}

// void dijkstraTest2() {
//     sys::Create(49, &NameServer);
//     int clockServerTid = sys::Create(50, &ClockServer);
//     sys::Create(30, &ConsoleServer);
//     int marklinServerTid = sys::Create(40, &MarklinServer);
//     int printerProprietorTid = sys::Create(35, &PrinterProprietor);

//     Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];
//     TrackNode track[TRACK_MAX];
//     init_trackb(track);
//     initializeTurnouts(turnouts, marklinServerTid, printerProprietorTid, clockServerTid);
//     initTrackSensorInfo(track, turnouts);  // sets every sensor's "nextSensor" according to track state

//     TrackNode* B2 = &track[17];

//     RingBuffer<TrackNode*, 1000> path;

//     for (int i = 0; i < 10; i++) {
//         uart_printf(CONSOLE, "\n\r");
//     }

//     computeShortestPath(B2, B2, path);

//     for (auto it = path.begin(); it != path.end(); ++it) {
//         TrackNode* node = *it;
//         uart_printf(CONSOLE, "%s->", node->name);
//     }
// }

// void dijkstraTest() {
//     TrackNode track[TRACK_MAX];
//     init_trackb(track);

//     TrackNode* B15 = &track[30];
//     TrackNode* E7 = &track[70];

//     RingBuffer<TrackNode*, 1000> path;

//     computeShortestPath(B15, E7, path);

//     TrackNode* node = nullptr;
//     ASSERT(path.size() == 6);

//     node = *path.front();
//     ASSERT(node->name == "E7");
//     path.pop();

//     node = *path.front();
//     ASSERT(node->name == "C13");
//     path.pop();

//     node = *path.front();
//     ASSERT(node->name == "MR11");
//     path.pop();

//     node = *path.front();
//     ASSERT(node->name == "BR14");
//     path.pop();

//     node = *path.front();
//     ASSERT(node->name == "A3");
//     path.pop();

//     node = *path.front();
//     ASSERT(node->name == "B15");
//     path.pop();

//     ASSERT(path.empty());
// }

void runPathfindingTest() {
    distanceMatrixTest();
    // dijkstraTest2();
}

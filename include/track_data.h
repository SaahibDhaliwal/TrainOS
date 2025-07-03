#ifndef __TRACK_DATA__
#define __TRACK_DATA__

#include <cstdint>

// maximum number of nodes in a layout
static constexpr int TRACK_MAX = 144;

// node kinds
enum class NodeType { NONE, SENSOR, BRANCH, MERGE, ENTER, EXIT };

// directions
static constexpr int DIR_AHEAD = 0;
static constexpr int DIR_STRAIGHT = 0;
static constexpr int DIR_CURVED = 1;

// forward declarations
struct TrackNode;
struct Edge;

// one edge in the graph
struct Edge {
    TrackNode* src = nullptr;   // source node
    TrackNode* dest = nullptr;  // destination node
    Edge* reverse = nullptr;    // reverse edge
    uint64_t dist = 0;          // in millimetres
};

// one node in the graph
struct TrackNode {
    const char* name = nullptr;  // e.g. "A1", "BR1", etc.
    NodeType type = NodeType::NONE;
    int num = -1;                  // sensor or switch number
    TrackNode* reverse = nullptr;  // same location, opposite direction
    Edge edge[2];                  // outgoing edges
    TrackNode* nextSensor = nullptr;
    uint64_t distToNextSensor = 0;
};

// track data initializers
// -- each takes a pre-allocated array of TRACK_MAX nodes
void init_tracka(TrackNode* track);
void init_trackb(TrackNode* track);

// helper: map sensor box ('A'/'B') and number to an index
int sensor_index(char box, int num);

#endif
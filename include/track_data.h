#ifndef __TRACK_DATA__
#define __TRACK_DATA__

typedef enum {
    NODE_NONE,
    NODE_SENSOR,
    NODE_BRANCH,
    NODE_MERGE,
    NODE_ENTER,
    NODE_EXIT,
} node_type;

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge track_edge;

struct track_edge {
    track_edge *reverse;
    track_node *src, *dest;
    int dist; /* in millimetres */
};

struct track_node {
    const char *name;
    node_type type;
    int num;             /* sensor or switch number */
    track_node *reverse; /* same location, but opposite direction */
    track_edge edge[2];
};

// The track initialization functions expect an array of this size.
#define TRACK_MAX 144

void init_tracka(track_node *track);
void init_trackb(track_node *track);

#endif
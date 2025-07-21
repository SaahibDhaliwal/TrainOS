#include "pathfinding.h"

#include <cstring>

#include "priority_queue.h"
#include "ring_buffer.h"
#include "unordered_map.h"

void computeShortestPath(TrackNode* source, TrackNode* target, RingBuffer<TrackNode*, 1000>& path) {
    uint64_t distances[TRACK_MAX];
    TrackNode* parents[TRACK_MAX];
    PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

    for (int i = 0; i < TRACK_MAX; i++) {
        distances[i] = UINT64_MAX;
        parents[i] = nullptr;
    }

    uint64_t sourceNum = source->id;
    distances[sourceNum] = 0;
    parents[sourceNum] = nullptr;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [curDist, curNode] = pq.top();
        pq.pop();

        if (curNode == target) break;  // found shortest path to target

        if (curDist > distances[curNode->id]) continue;  // stale pq entry

        if (curNode->type == NodeType::EXIT) continue;

        int dirs[2];
        int numberOfDirs = 0;

        if (curNode->type == NodeType::BRANCH) {
            dirs[numberOfDirs++] = DIR_STRAIGHT;
            dirs[numberOfDirs++] = DIR_CURVED;
        } else {
            dirs[numberOfDirs++] = DIR_AHEAD;
        }

        for (int i = 0; i < numberOfDirs; i += 1) {
            TrackEdge* edge = &curNode->edge[dirs[i]];
            TrackNode* destNode = edge->dest;
            uint64_t newDistToDest = curDist + edge->dist;

            ASSERT(destNode != nullptr);

            if (newDistToDest < distances[destNode->id]) {
                parents[destNode->id] = curNode;
                distances[destNode->id] = newDistToDest;

                if (curNode->type != NodeType::EXIT) {
                    pq.push({distances[destNode->id], destNode});
                }
            }
        }
    }

    TrackNode* cur = target;
    while (parents[cur->id]) {
        path.push(cur);
        cur = parents[cur->id];
    }
    path.push(cur);
}

void computeShortestDistancesFromSource(TrackNode* source, uint64_t distRow[TRACK_MAX]) {
    for (int i = 0; i < TRACK_MAX; i++) {
        distRow[i] = UINT64_MAX;
    }
    PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

    uint64_t sourceNum = source->id;
    distRow[sourceNum] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [curDist, curNode] = pq.top();
        pq.pop();

        if (curDist > distRow[curNode->id]) continue;  // stale pq entry
        if (curNode->type == NodeType::EXIT) continue;

        int dirs[2];
        int numberOfDirs = 0;

        if (curNode->type == NodeType::BRANCH) {
            dirs[numberOfDirs++] = DIR_STRAIGHT;
            dirs[numberOfDirs++] = DIR_CURVED;
        } else {
            dirs[numberOfDirs++] = DIR_AHEAD;
        }

        for (int i = 0; i < numberOfDirs; i += 1) {
            TrackEdge* edge = &curNode->edge[dirs[i]];
            TrackNode* destNode = edge->dest;
            uint64_t newDistToDest = curDist + edge->dist;

            ASSERT(destNode != nullptr);

            if (newDistToDest < distRow[destNode->id]) {
                distRow[destNode->id] = newDistToDest;

                if (curNode->type != NodeType::EXIT) {
                    pq.push({distRow[destNode->id], destNode});
                }
            }
        }
    }
}

void initializeDistanceMatrix(TrackNode* track, uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX]) {
    for (int i = 0; i < TRACK_MAX; ++i) {
        computeShortestDistancesFromSource(&track[i], distanceMatrix[i]);
    }
}

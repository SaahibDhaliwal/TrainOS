#include "pathfinding.h"

#include <cstring>

#include "printer_proprietor_protocol.h"
#include "priority_queue.h"
#include "ring_buffer.h"
#include "unordered_map.h"

// void computeShortestPath(TrackNode* source, TrackNode* target, RingBuffer<TrackNode*, 1000>& path) {
//     uint64_t distances[TRACK_MAX];
//     TrackNode* parents[TRACK_MAX];
//     PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

//     for (int i = 0; i < TRACK_MAX; i++) {
//         distances[i] = UINT64_MAX;
//         parents[i] = nullptr;
//     }

//     uint64_t sourceNum = source->id;
//     distances[sourceNum] = 0;
//     parents[sourceNum] = nullptr;
//     pq.push({0, source});

//     while (!pq.empty()) {
//         auto [curDist, curNode] = pq.top();
//         pq.pop();

//         if (curNode == target) break;  // found shortest path to target

//         if (curDist > distances[curNode->id]) continue;  // stale pq entry

//         if (curNode->type == NodeType::EXIT) continue;

//         int dirs[2];
//         int numberOfDirs = 0;

//         if (curNode->type == NodeType::BRANCH) {
//             dirs[numberOfDirs++] = DIR_STRAIGHT;
//             dirs[numberOfDirs++] = DIR_CURVED;
//         } else {
//             dirs[numberOfDirs++] = DIR_AHEAD;
//         }

//         for (int i = 0; i < numberOfDirs; i += 1) {
//             TrackEdge* edge = &curNode->edge[dirs[i]];
//             TrackNode* destNode = edge->dest;
//             uint64_t newDistToDest = curDist + edge->dist;

//             ASSERT(destNode != nullptr);

//             if (newDistToDest < distances[destNode->id]) {
//                 parents[destNode->id] = curNode;
//                 distances[destNode->id] = newDistToDest;

//                 if (curNode->type != NodeType::EXIT) {
//                     pq.push({distances[destNode->id], destNode});
//                 }
//             }
//         }
//     }

//     TrackNode* cur = target;
//     while (parents[cur->id]) {
//         path.push(cur);
//         cur = parents[cur->id];
//     }
//     path.push(cur);
// }

// void computeShortestDistancesFromSource(TrackNode* source, uint64_t distRow[TRACK_MAX]) {
//     for (int i = 0; i < TRACK_MAX; i++) {
//         distRow[i] = UINT64_MAX;
//     }
//     PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

//     uint64_t sourceNum = source->id;
//     distRow[sourceNum] = 0;
//     pq.push({0, source});

//     while (!pq.empty()) {
//         auto [curDist, curNode] = pq.top();
//         pq.pop();

//         if (curDist > distRow[curNode->id]) continue;  // stale pq entry
//         if (curNode->type == NodeType::EXIT) continue;

//         int dirs[2];
//         int numberOfDirs = 0;

//         if (curNode->type == NodeType::BRANCH) {
//             dirs[numberOfDirs++] = DIR_STRAIGHT;
//             dirs[numberOfDirs++] = DIR_CURVED;
//         } else {
//             dirs[numberOfDirs++] = DIR_AHEAD;
//         }

//         for (int i = 0; i < numberOfDirs; i += 1) {
//             TrackEdge* edge = &curNode->edge[dirs[i]];
//             TrackNode* destNode = edge->dest;
//             uint64_t newDistToDest = curDist + edge->dist;

//             ASSERT(destNode != nullptr);

//             if (newDistToDest < distRow[destNode->id]) {
//                 distRow[destNode->id] = newDistToDest;

//                 if (curNode->type != NodeType::EXIT) {
//                     pq.push({distRow[destNode->id], destNode});
//                 }
//             }
//         }
//     }
// }

uint64_t computeForwardShortestPathAvoidingZone(TrackNode* source, TrackNode* target,
                                                RingBuffer<TrackNode*, 1000>& path, TrainReservation* trainReservation,
                                                int printerTid, uint32_t zoneNum) {
    uint64_t distances[TRACK_MAX];
    TrackNode* parents[TRACK_MAX];
    PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

    for (int i = 0; i < TRACK_MAX; i++) {
        distances[i] = UINT64_MAX;
        parents[i] = nullptr;
    }

    if (source == target) {
        source = source->nextSensor;
    }
    printer_proprietor::debugPrintF(printerTid, "target node is: %s source is: %s", target->name, source->name);

    uint64_t sourceNum = source->id;
    distances[sourceNum] = 0;
    parents[sourceNum] = nullptr;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [curDist, curNode] = pq.top();
        pq.pop();

        if (curNode == target || curNode == target->reverse) break;  // found shortest path to target

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

            // if (trainReservation->isTrackNodeInZone(destNode, zoneNum)) continue;

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

    TrackNode* actualTarget = target;
    if (distances[target->reverse->id] < distances[target->id]) {
        actualTarget = target->reverse;
    }

    TrackNode* cur = actualTarget;

    while (parents[cur->id]) {
        path.push(cur);
        cur = parents[cur->id];
    }
    path.push(cur);

    ASSERT(actualTarget != nullptr);
    ASSERT(actualTarget->id >= 0 && actualTarget->id < TRACK_MAX);
    if (path.size() == 1) {
        printer_proprietor::debugPrintF(printerTid, "THE PATH IS JUST ONE NODE %s", (*path.front())->name);
        ASSERT(distances[actualTarget->id] == UINT64_MAX);
    }

    return distances[actualTarget->id];
}

uint64_t computeForwardShortestPath(TrackNode* source, TrackNode* target, RingBuffer<TrackNode*, 1000>& path,
                                    TrainReservation* trainReservation, int printerTid) {
    uint64_t distances[TRACK_MAX];
    TrackNode* parents[TRACK_MAX];
    PriorityQueue<std::pair<uint64_t, TrackNode*>, TRACK_MAX> pq;

    for (int i = 0; i < TRACK_MAX; i++) {
        distances[i] = UINT64_MAX;
        parents[i] = nullptr;
    }

    if (source == target) {
        source = source->nextSensor;
    }

    uint64_t sourceNum = source->id;
    distances[sourceNum] = 0;
    parents[sourceNum] = nullptr;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [curDist, curNode] = pq.top();
        pq.pop();

        if (curNode == target || curNode == target->reverse) break;  // found shortest path to target

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

            // if (trainReservation->isSectionReserved(destNode) != 0) continue;

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

    TrackNode* actualTarget = target;
    if (distances[target->reverse->id] < distances[target->id]) {
        actualTarget = target->reverse;
    }

    TrackNode* cur = actualTarget;

    while (parents[cur->id]) {
        path.push(cur);
        cur = parents[cur->id];
    }
    path.push(cur);

    ASSERT(actualTarget != nullptr);
    ASSERT(actualTarget->id >= 0 && actualTarget->id < TRACK_MAX);
    if (path.size() == 1) {
        printer_proprietor::debugPrintF(printerTid, "THE PATH IS JUST ONE NODE %s", (*path.front())->name);
        ASSERT(distances[actualTarget->id] == UINT64_MAX);
    }

    return distances[actualTarget->id];
}

PATH_FINDING_RESULT computeShortestPath(TrackNode* sourceOne, TrackNode* sourceTwo, TrackNode* target,
                                        RingBuffer<TrackNode*, 1000>& path, TrainReservation* trainReservation,
                                        int printerTid) {
    RingBuffer<TrackNode*, 1000> forwardPath;
    RingBuffer<TrackNode*, 1000> reversePath;

    printer_proprietor::debugPrintF(printerTid, "computing PATH WITH %s", sourceOne->name);
    uint64_t forwardPathLen = computeForwardShortestPath(sourceOne, target, forwardPath, trainReservation, printerTid);
    uint64_t reversePathLen = UINT64_MAX;
    if (sourceTwo) {
        printer_proprietor::debugPrintF(printerTid, "computing PATH WITH %s", sourceTwo->name);
        reversePathLen = computeForwardShortestPath(sourceTwo, target, reversePath, trainReservation, printerTid);
    }

    if (forwardPathLen == UINT64_MAX && reversePathLen == UINT64_MAX) {
        uart_printf(CONSOLE, "PAPA THERE IS NO PATH FROM: %s TO %s", sourceOne->name, target->name);
        return PATH_FINDING_RESULT::NO_PATH;
    } else if (reversePathLen == UINT64_MAX) {
        path = forwardPath;
        return PATH_FINDING_RESULT::FORWARD;
    } else if (forwardPathLen == UINT64_MAX) {
        path = reversePath;
        return PATH_FINDING_RESULT::REVERSE;
    } else {
        if (forwardPathLen < reversePathLen) {
            path = forwardPath;
            return PATH_FINDING_RESULT::FORWARD;
        } else {
            path = reversePath;
            return PATH_FINDING_RESULT::REVERSE;
        }
    }
}

void computeShortestDistancesFromSource(TrackNode* source, uint64_t distRow[TRACK_MAX]) {
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
    for (int i = 0; i < TRACK_MAX; i++) {
        for (int j = 0; j < TRACK_MAX; j++) {
            distanceMatrix[i][j] = UINT64_MAX;
        }
    }

    for (int i = 0; i < TRACK_MAX; ++i) {
        if (track[i].type == NodeType::EXIT) continue;
        computeShortestDistancesFromSource(&track[i], distanceMatrix[i]);
    }
}

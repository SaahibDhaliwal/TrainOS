#include "pathfinding.h"

#include "priority_queue.h"
#include "ring_buffer.h"
#include "unordered_map.h"

uint64_t nodeToNum(TrackNode* node) {
    return (static_cast<int>(node->type) * 100) + node->num;
}

void computeShortestPath(TrackNode* source, TrackNode* target, RingBuffer<TrackNode*, 100>& path) {
    UnorderedMap<uint64_t, uint64_t, 500> distances;
    UnorderedMap<uint64_t, TrackNode*, 500> parents;
    PriorityQueue<std::pair<uint64_t, TrackNode*>, 500> pq;

    uint64_t sourceNum = nodeToNum(source);
    distances[sourceNum] = 0;
    parents[sourceNum] = nullptr;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [curDist, curNode] = pq.top();
        pq.pop();

        if (curNode == target) break;  // found shortest path to target

        if (curDist != distances[nodeToNum(curNode)]) continue;  // stale pq entry

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
            uint64_t destNodeNum = nodeToNum(destNode);
            int64_t newDistToDest = curDist + edge->dist;

            ASSERT(destNode != nullptr);

            if (!distances.exists(destNodeNum) || (newDistToDest < distances[destNodeNum])) {
                parents[destNodeNum] = curNode;
                distances[destNodeNum] = newDistToDest;

                if (destNode->type != NodeType::EXIT) {
                    pq.push({distances[destNodeNum], destNode});
                }
            }
        }
    }

    TrackNode* cur = target;
    while (parents[nodeToNum(cur)]) {
        path.push(cur);
        cur = parents[nodeToNum(cur)];
    }
    path.push(cur);
}

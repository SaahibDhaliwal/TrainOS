#include "localization_helper.h"

#include "cstdint"
#include "test_utils.h"

// using namespace localization_server;
// reverse, nextsensor, 0
void setAllImpactedSensors(TrackNode* start, Turnout* turnouts, TrackNode* newNextSensor, int distance) {
    // did our recursion bring us to an impacted sensor?
    if (start != newNextSensor->reverse && start->type == NodeType::SENSOR) {
        start->reverse->distToNextSensor = distance;
        start->reverse->nextSensor = newNextSensor;
        // ASSERT(0, "reached sensor %s with next sensor as: %s", start->reverse->name, newNextSensor->name);
        return;
    }

    TrackNode* nextNode = start;  // might not be needed
    while (nextNode->type != NodeType::SENSOR || nextNode == newNextSensor->reverse) {
        if (nextNode->type == NodeType::BRANCH) {
            // go down each path and set an impacted sensor for each

            setAllImpactedSensors(nextNode->edge[DIR_STRAIGHT].dest, turnouts, newNextSensor,
                                  distance + nextNode->edge[DIR_STRAIGHT].dist);

            setAllImpactedSensors(nextNode->edge[DIR_CURVED].dest, turnouts, newNextSensor,
                                  distance + nextNode->edge[DIR_CURVED].dist);

            // after this, we can assume all those other sensors have been updated
            return;

        } else if (nextNode->type == NodeType::EXIT) {
            // ASSERT(0, "hit exit node after branch");
            return;
        } else {
            distance += nextNode->edge[DIR_AHEAD].dist;
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }

    // we get here if we reached a sensor and didn't need to branch
    // need to reverse since we're working backwards
    nextNode->reverse->distToNextSensor = distance;
    nextNode->reverse->nextSensor = newNextSensor;
    // ASSERT(0, "reached sensor %s with next sensor as: %s (no branch)", nextNode->reverse->name, newNextSensor->name);
    return;
}

// follows the track state to update the starting sensor's next sensor
//  note: start must be a sensor
void setNextSensor(TrackNode* start, Turnout* turnouts) {
    int mmTotalDist = start->edge[DIR_AHEAD].dist;
    TrackNode* nextNode = start->edge[DIR_AHEAD].dest;
    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR) {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
                mmTotalDist += nextNode->edge[DIR_CURVED].dist;
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                mmTotalDist += nextNode->edge[DIR_STRAIGHT].dist;
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::EXIT) {  // pretty sure ENTER nodes do have a next sensor!
            // ex: sensor A2, A14, A15 don't have a next sensor
            deadendFlag = true;
            break;
        } else {
            mmTotalDist += nextNode->edge[DIR_AHEAD].dist;
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }

    if (!deadendFlag) {
        start->nextSensor = nextNode;
        start->distToNextSensor = mmTotalDist;
    } else {
        start->nextSensor = nullptr;
        start->distToNextSensor = 0;
    }
}

// ensure your starting node is not a sensor
TrackNode* getNextSensor(TrackNode* start, Turnout* turnouts) {
    TrackNode* nextNode = nullptr;
    if (start->type != NodeType::BRANCH) {
        nextNode = start->edge[DIR_AHEAD].dest;
    } else if (turnouts[turnoutIdx(start->num)].state == SwitchState::STRAIGHT) {
        if (start->num == 153 || start->num == 155) {
            nextNode = start->reverse->edge[DIR_AHEAD].dest->reverse->edge[DIR_CURVED].dest;
        } else {
            nextNode = start->edge[DIR_CURVED].dest;
        }
    } else {
        nextNode = start->edge[DIR_CURVED].dest;
    }

    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR) {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::EXIT) {  // pretty sure ENTER nodes do have a next sensor!
            // ex: sensor A2, A14, A15 don't have a next sensor
            deadendFlag = true;
            break;
        } else {
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }
    if (!deadendFlag) {
        return nextNode;
    } else {
        // ASSERT(0, "There was no next sensor");
        return nullptr;
    }
}

// ensure your starting node is not a sensor
TrackNode* getNextRealSensor(TrackNode* start, Turnout* turnouts) {
    TrackNode* nextNode = nullptr;
    if (start->type != NodeType::BRANCH) {
        nextNode = start->edge[DIR_AHEAD].dest;
    } else if (turnouts[turnoutIdx(start->num)].state == SwitchState::STRAIGHT) {
        if (start->num == 153 || start->num == 155) {
            nextNode = start->reverse->edge[DIR_AHEAD].dest->reverse->edge[DIR_CURVED].dest;
        } else {
            nextNode = start->edge[DIR_CURVED].dest;
        }
    } else {
        nextNode = start->edge[DIR_CURVED].dest;
    }

    bool deadendFlag = false;
    while (nextNode->type != NodeType::SENSOR && nextNode->name[0] != 'F') {
        if (nextNode->type == NodeType::BRANCH) {
            // if we hit a branch node, there are two different edges to take
            // so look at our inital turnout state to know which sensor is next
            if (turnouts[turnoutIdx(nextNode->num)].state == SwitchState::CURVED) {
                nextNode = nextNode->edge[DIR_CURVED].dest;
            } else {
                nextNode = nextNode->edge[DIR_STRAIGHT].dest;
            }
        } else if (nextNode->type == NodeType::EXIT) {  // pretty sure ENTER nodes do have a next sensor!
            // ex: sensor A2, A14, A15 don't have a next sensor
            deadendFlag = true;
            break;
        } else {
            nextNode = nextNode->edge[DIR_AHEAD].dest;
        }
    }
    if (!deadendFlag) {
        return nextNode;
    } else {
        // ASSERT(0, "There was no next sensor");
        return nullptr;
    }
}

#define MAX_SENSORS 80
void initTrackSensorInfo(TrackNode* track, Turnout* turnouts) {
    for (int i = 0; i < MAX_SENSORS; i++) {  // only need to look through first 80 indices (16 * 5)
        // do a dfs for the next sensor
        setNextSensor(&track[i], turnouts);
    }
// also our new sensors
#if defined(TRACKA)
    for (int i = 144; i < TRACK_MAX; i++) {  // only need to look through first 80 indices (16 * 5)
        // do a dfs for the next sensor
        setNextSensor(&track[i], turnouts);
    }
#else
    for (int i = 140; i < TRACK_MAX; i++) {  // only need to look through first 80 indices (16 * 5)
        // do a dfs for the next sensor
        setNextSensor(&track[i], turnouts);
    }

    ASSERT(track[38].nextSensor->name[0] == 'F' && track[38].nextSensor->name[1] == '4',
           "THIS WILL FAIL IF YOU CHANGE OUR INITIAL CONFIG sensors were mapped wrong. C7 next sensor name: %s num: %d",
           track[38].nextSensor->name,
           track[38].nextSensor->num);  // c7
    ASSERT(track[38].nextSensor->num == 4, "sensors were mapped wrong. C7 next sensor num: %s num: %d",
           track[38].nextSensor->num);  // c7
    // ASSERT(track[35].nextSensor->name == "F1", "sensors were mapped wrong. C4 next sensor name: %s num: %d",
    //        track[38].nextSensor->name, track[38].nextSensor->num);  // c4
    // ASSERT(track[36].nextSensor->name == "F2", "sensors were mapped wrong");  // c5
    // ASSERT(track[75].nextSensor->name == "F3", "sensors were mapped wrong");  // e12

#endif
}

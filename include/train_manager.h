#ifndef __TRAIN_MANAGER__
#define __TRAIN_MANAGER__

#include <cstdint>
#include <utility>

#include "config.h"
#include "localization_server_protocol.h"
#include "ring_buffer.h"
#include "track_data.h"
#include "train_task.h"
#include "turnout.h"
#include "zone.h"

#define NODE_MAX 4

namespace localization_server {

struct Train {
    bool active;
    bool isReversing;
    uint8_t speed;
    uint8_t id;
    uint64_t velocity;
    // these are all ints associated with the array
    TrackNode* nodeAhead;
    TrackNode* nodeBehind;
    TrackNode* sensorAhead;
    TrackNode* realSensorAhead;
    // int64_t sensorAheadMicros;
    TrackNode* sensorBehind;
    int trackCount = 0;

    // stopping
    bool stopping;
    uint64_t stoppingDistance;
    TrackNode* targetNode;
    TrackNode* stoppingSensor;
    uint64_t whereToIssueStop;
    TrackNode* sensorWhereSpeedChangeStarted;
    RingBuffer<TrackNode*, 1000> path;
    // RingBuffer<TrackNode*, 1000> backwardsPath;
};
}  // namespace localization_server
class TrainManager {
   private:
    localization_server::Train trains[Config::MAX_TRAINS];
    uint32_t trainTasks[Config::MAX_TRAINS];
    TrackNode track[TRACK_MAX];
    Turnout turnouts[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT];
    TrainReservation trainReservation;

    int marklinServerTid;
    int printerProprietorTid;
    int clockServerTid;
    uint32_t stoppingTid;
    uint32_t turnoutNotifierTid;

    uint64_t prevMicros = 0;
    uint64_t laps = 0;
    uint32_t reverseTid = 0;
    int stopTrainIndex = 0;
    uint32_t delayTicks = 0;
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;  // pair<mm(deltaD), micros(deltaT)>
    int64_t prevSensorMicros = 0;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;
    RingBuffer<std::pair<char, char>, 100> turnoutQueue;
    bool stopTurnoutNotifier = false;
    RingBuffer<int, Config::MAX_TRAINS> reversingTrains;
    TrackNode* train13[NODE_MAX];
    TrackNode* train14[NODE_MAX];
    localization_server::Train* playerTrain = nullptr;
    int playerTrainIndex;
    localization_server::BranchDirection playerDirection = localization_server::BranchDirection::STRAIGHT;
    UnorderedMap<TrackNode*, localization_server::BranchDirection, SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT> switchMap;

    void generatePath(localization_server::Train* curTrain, int targetTrackNodeIdx, int signedOffset);
    void initSwitchMap(TrackNode* track);
    void notifierPop(TrackNode* nextNode);

   public:
    TrainManager(int marklinServerTid, int printerProprietorTid, int clockServerTid, uint32_t turnoutNotifierTid);
    void setTrainSpeed(unsigned int trainSpeed, unsigned int trainNumber);
    void processSensorReading(char* receiveBuffer);
    // void processReverse();
    void processTurnoutNotifier();
    void processStopping();
    void processTrainRequest(char* receiveBuffer, uint32_t clientTid);
    void reverseTrain(int trainIndex);
    void setTrainStop(char* receiveBuffer);
    // getters
    uint32_t getReverseTid();
    uint32_t getSmallestTrainTid();
    uint32_t getLargestTrainTid();
    TrackNode* getTrack();
    Turnout* getTurnouts();
    TrackNode* trainIndexToTrackNode(int trainIndex, int count);

    void initializeTrain(int trainIndex, Sensor initSensor);
    void initializePlayer(int trainIndex, Sensor initSensor);
    void fakeSensorHit(int trainIndex);

    void processPlayerInput(char input);
};

#endif
#ifndef __TRAIN_MANAGER__
#define __TRAIN_MANAGER__

#include <cstdint>
#include <utility>

#include "config.h"
#include "ring_buffer.h"
#include "track_data.h"
#include "train.h"
#include "turnout.h"
#include "zone.h"

#define NODE_MAX 7
class TrainManager {
   private:
    Train trains[MAX_TRAINS];
    uint32_t trainTasks[MAX_TRAINS];
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
    uint64_t newSensorsPassed = 0;
    uint32_t reverseTid = 0;
    int stopTrainIndex = 0;
    uint32_t delayTicks = 0;
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;  // pair<mm(deltaD), micros(deltaT)>
    int64_t prevSensorMicros = 0;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;
    RingBuffer<std::pair<char, char>, 100> turnoutQueue;
    bool stopTurnoutNotifier = false;
    RingBuffer<int, MAX_TRAINS> reversingTrains;
    TrackNode* train13[NODE_MAX];
    TrackNode* train14[NODE_MAX];

   public:
    TrainManager(int marklinServerTid, int printerProprietorTid, int clockServerTid, uint32_t turnoutNotifierTid);
    void processInputCommand(char* receiveBuffer);
    void processSensorReading(char* receiveBuffer);
    void processReverse();
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
};

#endif
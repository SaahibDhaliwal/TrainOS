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

class TrainManager {
   private:
    Train trains[MAX_TRAINS];
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
    int64_t prevSensorPredicitionMicros = 0;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;
    RingBuffer<std::pair<char, char>, 100> turnoutQueue;
    bool stopTurnoutNotifier = false;
    RingBuffer<int, MAX_TRAINS> reversingTrains;

   public:
    TrainManager(int marklinServerTid, int printerProprietorTid, int clockServerTid, uint32_t stoppingTid,
                 uint32_t turnoutNotifierTid);
    void processInputCommand(char* receiveBuffer);
    void processSensorReading(char* receiveBuffer);
    void processReverse();
    void processTurnoutNotifier();
    void processStopping();
    // getters
    uint32_t getReverseTid();
};

#endif
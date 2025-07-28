#ifndef __TRAIN_CLASS__
#define __TRAIN_CLASS__

#include <stdint.h>

#include <utility>

#include "ring_buffer.h"
#include "track_data.h"
#include "train_protocol.h"
#include "zone.h"

class Train {
   private:
    uint32_t parentTid;
    uint32_t marklinServerTid;
    uint32_t printerProprietorTid;
    uint32_t clockServerTid;
    uint32_t updaterTid;
    uint32_t stopNotifierTid;
    uint32_t reverseTid;

    TrackNode track[TRACK_MAX];
    uint64_t distanceMatrix[TRACK_MAX][TRACK_MAX];

    // ************ STATIC TRAIN STATE *************
    uint8_t speed = 0;              // Marklin Box Unit
    uint64_t stoppingDistance = 0;  // mm

    //     uint64_t slowingDown = 0;
    //     bool stopped = true;
    bool isForward = true;

    unsigned int myTrainNumber;
    int trainIndex;
    const char* trainColour;

    // ********** REAL-TIME TRAIN STATE ************
    uint64_t velocityEstimate = 0;  // in micrometers per microsecond (µm/µs) with a *1000 for decimals
    RingBuffer<std::pair<uint64_t, uint64_t>, 10> velocitySamples;
    uint64_t velocitySamplesNumeratorSum = 0;
    uint64_t velocitySamplesDenominatorSum = 0;

    Sensor sensorAhead;                            // sensor ahead of train
    Sensor sensorBehind;                           // sensor behind train
    Sensor stopSensor;                             // sensor we are waiting to hit
    Sensor targetSensor;                           // sensor we are waiting to hit
    uint64_t stopSensorOffset = 0;                 // mm, static
    uint64_t distanceToSensorAhead = 0;            // mm, static
    int64_t distRemainingToSensorAhead = 0;        // mm, dynamic
    uint64_t distTravelledSinceLastSensorHit = 0;  // mm, dynamic

    bool isAccelerating = false;
    uint64_t accelerationStartTime = 0;  // micros
    uint64_t totalAccelerationTime = 0;  // micros

    bool isReversing = false;
    uint64_t reversingStartTime = 0;  // micros
    uint64_t totalReversingTime = 0;  // micros

    bool isSlowingDown = false;
    bool isStopped = true;
    uint64_t stopStartTime = 0;          // micros
    uint64_t totalStoppingTime = 0;      // micros
    uint64_t velocityBeforeSlowing = 0;  // micros

    bool reachedDestination = false;

    // ********** SENSOR MANAGEMENT ****************

    uint64_t prevSensorHitMicros = 0;
    uint64_t prevDistance = 0;
    int64_t prevSensorPredicitionMicros = 0;
    int64_t sensorAheadMicros = 0;

    // *********** UPDATE MANAGEMENT ***************
    uint64_t prevUpdateMicros = 0;

    // ********* RESERVATION MANAGEMENT ************
    RingBuffer<ReservedZone, 32> reservedZones;
    RingBuffer<ZoneExit, 32> zoneExits;

    bool recentZoneAddedFlag = false;

    bool firstReservationFailure = false;

    Sensor zoneEntraceSensorAhead;                       // sensor ahead of train marking a zone entrace
    uint64_t distanceToZoneEntraceSensorAhead = 0;       // mm, static
    int64_t distRemainingToZoneEntranceSensorAhead = 0;  // mm, dynamic

    /////////////////////////////////////////////////////////////////////

    void newSensorHitFromStopped(Sensor sensor);

    bool attemptReservation(int64_t curMicros);

    void initialSensorHit(Sensor curSensor);
    void regularSensorHit(uint64_t curMicros, Sensor curSensor);
    void makeReservation();
    void makeInitialReservation();

    void accelerateTrain();
    void decelerateTrain();
    uint64_t predictNextSensorHitTimeMicros();
    void initReservation();

   public:
    Train(unsigned int myTrainNumber, int parentTid, uint32_t printerProprietorTid, uint32_t marklinServerTid,
          uint32_t clockServerTid, uint32_t updaterTid, uint32_t stopNotifierTid);

    // getters
    uint32_t getReverseTid();
    Sensor getStopSensor();

    void setTrainSpeed(unsigned int trainSpeed);
    // on sensor hit
    void processSensorHit(Sensor currentSensor, Sensor sensorAhead, uint64_t distanceToSensorAhead);

    // reservation stuff
    bool shouldTrainReserveNextZone();

    void tryToFreeZones(uint64_t distTravelledSinceLastSensorHit = 0);
    //
    void reverseCommand();
    //
    void newStopLocation(Sensor stopSensor, Sensor targetSensor, uint64_t offset);
    void updateState();
    //
    void updateVelocityWithAcceleration(int64_t curMicros);
    void updateVelocityWithDeceleration(int64_t curMicros);

    void finishReverse();

    uint64_t getReverseDelayTicks();
    uint64_t getStopDelayTicks(int64_t curMicros);

    void stop();
};

#endif
#include "zone.h"

#include "printer_proprietor_protocol.h"

void TrainReservation::initialize(TrackNode* track, uint32_t printerProprietorTid) {
    this->printerProprietorTid = printerProprietorTid;
    for (int i = 0; i < ZONE_COUNT + 1; i++) {
        zoneArray[i].zoneNum = i;
    }
#if defined(TRACKA)
    initZoneA(track);
#else
    initZoneB(track);
#endif
    // zoneArray[21].reserved = true;
}

uint32_t TrainReservation::trackNodeToZoneNum(TrackNode* track) {
    ZoneSegment* result = mapchecker(track);
    return result->zoneNum;
}

// for routing I assume this is needed
int TrainReservation::isSectionReserved(TrackNode* start) {
    ZoneSegment* result = mapchecker(start);
    if (result->reserved) {
        return result->trainNumber;
    }
    return false;
}

ZoneSegment* TrainReservation::mapchecker(TrackNode* entrySensor) {
    ZoneSegment** search = zoneMap.get(entrySensor);
    ASSERT(search != nullptr, "That node doesn't exist in the map. node: %s", entrySensor->name);
    ZoneSegment* result = *search;
    ASSERT(result != nullptr, "We allocated a nullptr in the map, oops");
    return result;
}

// should be done on sensor tracknodes
bool TrainReservation::reservationAttempt(TrackNode* entrySensor, unsigned int trainNumber) {
    ASSERT(entrySensor->type == NodeType::SENSOR, "gave node that was instead named: %s", entrySensor->name);

    ZoneSegment* result = mapchecker(entrySensor);
    if (!result->reserved) {
        result->reserved = true;
        result->trainNumber = trainNumber;
        result->previousZone = mapchecker(entrySensor->reverse);

        // should also check if their reservation is path based and they need to reserve more stuff
        // i think this has to be done with the track state
        // so localization server receives a reservation request, it will check if its a special one,
        // if it is, follow the state of switches to get the next node
        // ex: in zone 12 (by sw 15), going to reserve 8
        // train asks for 8, server checks if they also plan on going to zone 6 (switch state)
        // if so, try and reserve 6 and then 8
        // if either fails, stop and unreserve

        return true;
    } else if (result->trainNumber == trainNumber) {
        // this requester already has it reserved
        return true;
    }
    return false;
}

bool TrainReservation::freeReservation(TrackNode* sensor, unsigned int trainNumber) {
    ZoneSegment** search = zoneMap.get(sensor);
    ASSERT(search != nullptr, "That node doesn't exist in the map. Are you sure it wasn't an ex/en?");
    ZoneSegment* result = *search;
    ASSERT(result != nullptr, "We allocated a nullptr in the map, oops");

    // printer_proprietor::debugPrintF(printerProprietorTid, "TRYING TO FREE ZONE %d, WITH SENSOR %s", result->zoneNum,
    //                                 sensor->name);

    // printer_proprietor::debugPrintF(printerProprietorTid, "RESULT TRIN NUMBER: %d, ACTUAL TRAIN NUMBER: %d",
    //                                 result->trainNumber, trainNumber);

    if (result->trainNumber == trainNumber) {
        result->reserved = false;
        return true;
    }

    // ASSERT(0, "Train %s tried freeing a reservation that wasn't theirs", trainNumber);
    return false;
}

void TrainReservation::updateReservation(TrackNode* sensor, unsigned int trainNumber, ReservationType reservation) {
    ZoneSegment* result = mapchecker(sensor);
    ASSERT(result->trainNumber == trainNumber, "Somebody tried updating a reservation they didn't own");
    result->reservationType = reservation;
}

ReservationType reservationFromByte(char c) {
    return (c < static_cast<char>(ReservationType::COUNT)) ? static_cast<ReservationType>(c)
                                                           : ReservationType::UNKNOWN_TYPE;
}

char toByte(ReservationType r) {
    return static_cast<char>(r);
}

void TrainReservation::initZoneA(TrackNode* trackArray) {
    // -------------------- A
    zoneMap.insert(&trackArray[0], &zoneArray[32]);
    zoneMap.insert(&trackArray[1], &zoneArray[31]);
    zoneMap.insert(&trackArray[2], &zoneArray[22]);
    zoneMap.insert(&trackArray[3], &zoneArray[19]);
    zoneMap.insert(&trackArray[4], &zoneArray[5]);
    zoneMap.insert(&trackArray[5], &zoneArray[4]);
    zoneMap.insert(&trackArray[6], &zoneArray[3]);
    zoneMap.insert(&trackArray[7], &zoneArray[5]);
    zoneMap.insert(&trackArray[8], &zoneArray[2]);
    zoneMap.insert(&trackArray[9], &zoneArray[5]);
    zoneMap.insert(&trackArray[10], &zoneArray[5]);
    zoneMap.insert(&trackArray[11], &zoneArray[1]);
    zoneMap.insert(&trackArray[12], &zoneArray[32]);
    zoneMap.insert(&trackArray[13], &zoneArray[30]);
    zoneMap.insert(&trackArray[14], &zoneArray[29]);
    zoneMap.insert(&trackArray[15], &zoneArray[32]);
    // -------------------- B
    zoneMap.insert(&trackArray[16], &zoneArray[14]);
    zoneMap.insert(&trackArray[17], &zoneArray[13]);
    zoneMap.insert(&trackArray[18], &zoneArray[17]);
    zoneMap.insert(&trackArray[19], &zoneArray[13]);
    zoneMap.insert(&trackArray[20], &zoneArray[26]);
    zoneMap.insert(&trackArray[21], &zoneArray[25]);
    zoneMap.insert(&trackArray[22], &zoneArray[2]);
    zoneMap.insert(&trackArray[23], &zoneArray[2]);  // should exit nodes be in zone?
    zoneMap.insert(&trackArray[24], &zoneArray[4]);
    zoneMap.insert(&trackArray[25], &zoneArray[4]);
    zoneMap.insert(&trackArray[26], &zoneArray[3]);
    zoneMap.insert(&trackArray[27], &zoneArray[3]);
    zoneMap.insert(&trackArray[28], &zoneArray[20]);
    zoneMap.insert(&trackArray[29], &zoneArray[18]);
    zoneMap.insert(&trackArray[30], &zoneArray[19]);
    zoneMap.insert(&trackArray[31], &zoneArray[12]);
    // -------------------- C
    zoneMap.insert(&trackArray[32], &zoneArray[17]);
    zoneMap.insert(&trackArray[33], &zoneArray[20]);
    zoneMap.insert(&trackArray[34], &zoneArray[7]);
    zoneMap.insert(&trackArray[35], &zoneArray[6]);
    zoneMap.insert(&trackArray[36], &zoneArray[8]);
    zoneMap.insert(&trackArray[37], &zoneArray[12]);
    zoneMap.insert(&trackArray[38], &zoneArray[6]);
    zoneMap.insert(&trackArray[39], &zoneArray[5]);
    zoneMap.insert(&trackArray[40], &zoneArray[12]);
    zoneMap.insert(&trackArray[41], &zoneArray[13]);
    zoneMap.insert(&trackArray[42], &zoneArray[25]);
    zoneMap.insert(&trackArray[43], &zoneArray[22]);
    zoneMap.insert(&trackArray[44], &zoneArray[33]);
    zoneMap.insert(&trackArray[45], &zoneArray[32]);
    zoneMap.insert(&trackArray[46], &zoneArray[9]);
    zoneMap.insert(&trackArray[47], &zoneArray[8]);
    // -------------------- D
    zoneMap.insert(&trackArray[48], &zoneArray[20]);
    zoneMap.insert(&trackArray[49], &zoneArray[24]);
    zoneMap.insert(&trackArray[50], &zoneArray[27]);
    zoneMap.insert(&trackArray[51], &zoneArray[26]);
    zoneMap.insert(&trackArray[52], &zoneArray[28]);
    zoneMap.insert(&trackArray[53], &zoneArray[21]);
    zoneMap.insert(&trackArray[54], &zoneArray[21]);
    zoneMap.insert(&trackArray[55], &zoneArray[34]);
    zoneMap.insert(&trackArray[56], &zoneArray[11]);
    zoneMap.insert(&trackArray[57], &zoneArray[21]);
    zoneMap.insert(&trackArray[58], &zoneArray[9]);
    zoneMap.insert(&trackArray[59], &zoneArray[10]);
    zoneMap.insert(&trackArray[60], &zoneArray[14]);
    zoneMap.insert(&trackArray[61], &zoneArray[15]);
    zoneMap.insert(&trackArray[62], &zoneArray[18]);
    zoneMap.insert(&trackArray[63], &zoneArray[15]);
    // -------------------- E
    zoneMap.insert(&trackArray[64], &zoneArray[20]);
    zoneMap.insert(&trackArray[65], &zoneArray[23]);
    zoneMap.insert(&trackArray[66], &zoneArray[24]);
    zoneMap.insert(&trackArray[67], &zoneArray[27]);
    zoneMap.insert(&trackArray[68], &zoneArray[28]);
    zoneMap.insert(&trackArray[69], &zoneArray[27]);
    zoneMap.insert(&trackArray[70], &zoneArray[34]);
    zoneMap.insert(&trackArray[71], &zoneArray[33]);
    zoneMap.insert(&trackArray[72], &zoneArray[21]);
    zoneMap.insert(&trackArray[73], &zoneArray[16]);
    zoneMap.insert(&trackArray[74], &zoneArray[11]);
    zoneMap.insert(&trackArray[75], &zoneArray[10]);
    zoneMap.insert(&trackArray[76], &zoneArray[15]);
    zoneMap.insert(&trackArray[77], &zoneArray[16]);
    zoneMap.insert(&trackArray[78], &zoneArray[25]);
    zoneMap.insert(&trackArray[79], &zoneArray[23]);
    // ------  BR1 ---------
    zoneMap.insert(&trackArray[80], &zoneArray[5]);
    zoneMap.insert(&trackArray[81], &zoneArray[5]);
    // ------  2 ---------
    zoneMap.insert(&trackArray[82], &zoneArray[5]);
    zoneMap.insert(&trackArray[83], &zoneArray[5]);
    // ------  3 ---------
    zoneMap.insert(&trackArray[84], &zoneArray[5]);
    zoneMap.insert(&trackArray[85], &zoneArray[5]);
    // ------  4 ---------
    zoneMap.insert(&trackArray[86], &zoneArray[32]);
    zoneMap.insert(&trackArray[87], &zoneArray[32]);
    // ------  5 ---------
    zoneMap.insert(&trackArray[88], &zoneArray[6]);
    zoneMap.insert(&trackArray[89], &zoneArray[6]);
    // ------  6 ---------
    zoneMap.insert(&trackArray[90], &zoneArray[8]);
    zoneMap.insert(&trackArray[91], &zoneArray[8]);
    // ------  7 ---------
    zoneMap.insert(&trackArray[92], &zoneArray[10]);
    zoneMap.insert(&trackArray[93], &zoneArray[10]);
    // ------  8 ---------
    zoneMap.insert(&trackArray[94], &zoneArray[21]);
    zoneMap.insert(&trackArray[95], &zoneArray[21]);
    // ------  9 ---------
    zoneMap.insert(&trackArray[96], &zoneArray[21]);
    zoneMap.insert(&trackArray[97], &zoneArray[21]);
    // ------  10 ---------
    zoneMap.insert(&trackArray[98], &zoneArray[27]);
    zoneMap.insert(&trackArray[99], &zoneArray[27]);
    // ------  11 ---------
    zoneMap.insert(&trackArray[100], &zoneArray[32]);
    zoneMap.insert(&trackArray[101], &zoneArray[32]);
    // ------  12 ---------
    zoneMap.insert(&trackArray[102], &zoneArray[32]);
    zoneMap.insert(&trackArray[103], &zoneArray[32]);
    // ------  13 ---------
    zoneMap.insert(&trackArray[104], &zoneArray[25]);
    zoneMap.insert(&trackArray[105], &zoneArray[25]);
    // ------  14 ---------
    zoneMap.insert(&trackArray[106], &zoneArray[22]);
    zoneMap.insert(&trackArray[107], &zoneArray[22]);
    // ------  15 ---------
    zoneMap.insert(&trackArray[108], &zoneArray[12]);
    zoneMap.insert(&trackArray[109], &zoneArray[12]);
    // ------  16 ---------
    zoneMap.insert(&trackArray[110], &zoneArray[13]);
    zoneMap.insert(&trackArray[111], &zoneArray[13]);
    // ------  17 ---------
    zoneMap.insert(&trackArray[112], &zoneArray[15]);
    zoneMap.insert(&trackArray[113], &zoneArray[15]);
    // ------  18 ---------
    zoneMap.insert(&trackArray[114], &zoneArray[6]);
    zoneMap.insert(&trackArray[115], &zoneArray[6]);
    // ------  153 ---------
    zoneMap.insert(&trackArray[116], &zoneArray[20]);
    zoneMap.insert(&trackArray[117], &zoneArray[20]);
    // ------  154 ---------
    zoneMap.insert(&trackArray[118], &zoneArray[20]);
    zoneMap.insert(&trackArray[119], &zoneArray[20]);
    // ------  155 ---------
    zoneMap.insert(&trackArray[120], &zoneArray[20]);
    zoneMap.insert(&trackArray[121], &zoneArray[20]);
    // ------  156 ---------
    zoneMap.insert(&trackArray[122], &zoneArray[20]);
    zoneMap.insert(&trackArray[123], &zoneArray[20]);
    // ------  EN / EX---------
    // // ------  1 ---------
    zoneMap.insert(&trackArray[124], &zoneArray[20]);
    zoneMap.insert(&trackArray[125], &zoneArray[20]);
    // ------  2 ---------
    zoneMap.insert(&trackArray[126], &zoneArray[20]);
    zoneMap.insert(&trackArray[127], &zoneArray[20]);
    // ------  3 ---------
    zoneMap.insert(&trackArray[128], &zoneArray[7]);
    zoneMap.insert(&trackArray[129], &zoneArray[7]);
    // ------  4 ---------
    zoneMap.insert(&trackArray[130], &zoneArray[30]);
    zoneMap.insert(&trackArray[131], &zoneArray[30]);
    // ------  5 ---------
    zoneMap.insert(&trackArray[132], &zoneArray[31]);
    zoneMap.insert(&trackArray[133], &zoneArray[31]);
    // ------  6 ---------
    zoneMap.insert(&trackArray[134], &zoneArray[29]);
    zoneMap.insert(&trackArray[135], &zoneArray[29]);
    // ------  7 ---------
    zoneMap.insert(&trackArray[136], &zoneArray[2]);
    zoneMap.insert(&trackArray[137], &zoneArray[2]);
    // ------  8 ---------
    zoneMap.insert(&trackArray[138], &zoneArray[1]);
    zoneMap.insert(&trackArray[139], &zoneArray[1]);
    // ------  9 ---------
    zoneMap.insert(&trackArray[140], &zoneArray[4]);
    zoneMap.insert(&trackArray[141], &zoneArray[4]);
    // ------  10 ---------
    zoneMap.insert(&trackArray[142], &zoneArray[3]);
    zoneMap.insert(&trackArray[143], &zoneArray[3]);

    zoneMap.insert(&trackArray[144], &zoneArray[8]);
    zoneMap.insert(&trackArray[145], &zoneArray[6]);
    zoneMap.insert(&trackArray[146], &zoneArray[6]);
    zoneMap.insert(&trackArray[147], &zoneArray[10]);
    zoneMap.insert(&trackArray[148], &zoneArray[32]);
    zoneMap.insert(&trackArray[149], &zoneArray[22]);
}

void TrainReservation::initZoneB(TrackNode* trackArray) {
    // -------------------- A
    zoneMap.insert(&trackArray[0], &zoneArray[32]);
    zoneMap.insert(&trackArray[1], &zoneArray[31]);
    zoneMap.insert(&trackArray[2], &zoneArray[22]);
    zoneMap.insert(&trackArray[3], &zoneArray[19]);
    zoneMap.insert(&trackArray[4], &zoneArray[5]);
    zoneMap.insert(&trackArray[5], &zoneArray[4]);
    zoneMap.insert(&trackArray[6], &zoneArray[3]);
    zoneMap.insert(&trackArray[7], &zoneArray[5]);
    zoneMap.insert(&trackArray[8], &zoneArray[2]);
    zoneMap.insert(&trackArray[9], &zoneArray[5]);
    zoneMap.insert(&trackArray[10], &zoneArray[5]);
    zoneMap.insert(&trackArray[11], &zoneArray[1]);
    zoneMap.insert(&trackArray[12], &zoneArray[32]);
    zoneMap.insert(&trackArray[13], &zoneArray[30]);
    zoneMap.insert(&trackArray[14], &zoneArray[1]);
    zoneMap.insert(&trackArray[15], &zoneArray[32]);
    // -------------------- B
    zoneMap.insert(&trackArray[16], &zoneArray[14]);
    zoneMap.insert(&trackArray[17], &zoneArray[13]);
    zoneMap.insert(&trackArray[18], &zoneArray[17]);
    zoneMap.insert(&trackArray[19], &zoneArray[13]);
    zoneMap.insert(&trackArray[20], &zoneArray[26]);
    zoneMap.insert(&trackArray[21], &zoneArray[25]);
    zoneMap.insert(&trackArray[22], &zoneArray[2]);
    zoneMap.insert(&trackArray[23], &zoneArray[2]);
    zoneMap.insert(&trackArray[24], &zoneArray[4]);
    zoneMap.insert(&trackArray[25], &zoneArray[4]);
    zoneMap.insert(&trackArray[26], &zoneArray[3]);
    zoneMap.insert(&trackArray[27], &zoneArray[3]);
    zoneMap.insert(&trackArray[28], &zoneArray[20]);
    zoneMap.insert(&trackArray[29], &zoneArray[18]);
    zoneMap.insert(&trackArray[30], &zoneArray[19]);
    zoneMap.insert(&trackArray[31], &zoneArray[12]);
    // -------------------- C
    zoneMap.insert(&trackArray[32], &zoneArray[17]);
    zoneMap.insert(&trackArray[33], &zoneArray[20]);
    zoneMap.insert(&trackArray[34], &zoneArray[7]);
    zoneMap.insert(&trackArray[35], &zoneArray[6]);
    zoneMap.insert(&trackArray[36], &zoneArray[8]);
    zoneMap.insert(&trackArray[37], &zoneArray[12]);
    zoneMap.insert(&trackArray[38], &zoneArray[6]);
    zoneMap.insert(&trackArray[39], &zoneArray[5]);
    zoneMap.insert(&trackArray[40], &zoneArray[12]);
    zoneMap.insert(&trackArray[41], &zoneArray[13]);
    zoneMap.insert(&trackArray[42], &zoneArray[25]);
    zoneMap.insert(&trackArray[43], &zoneArray[22]);
    zoneMap.insert(&trackArray[44], &zoneArray[33]);
    zoneMap.insert(&trackArray[45], &zoneArray[32]);
    zoneMap.insert(&trackArray[46], &zoneArray[9]);
    zoneMap.insert(&trackArray[47], &zoneArray[8]);
    // -------------------- D
    zoneMap.insert(&trackArray[48], &zoneArray[20]);
    zoneMap.insert(&trackArray[49], &zoneArray[24]);
    zoneMap.insert(&trackArray[50], &zoneArray[27]);
    zoneMap.insert(&trackArray[51], &zoneArray[26]);
    zoneMap.insert(&trackArray[52], &zoneArray[28]);
    zoneMap.insert(&trackArray[53], &zoneArray[21]);
    zoneMap.insert(&trackArray[54], &zoneArray[21]);
    zoneMap.insert(&trackArray[55], &zoneArray[34]);
    zoneMap.insert(&trackArray[56], &zoneArray[11]);
    zoneMap.insert(&trackArray[57], &zoneArray[21]);
    zoneMap.insert(&trackArray[58], &zoneArray[9]);
    zoneMap.insert(&trackArray[59], &zoneArray[10]);
    zoneMap.insert(&trackArray[60], &zoneArray[14]);
    zoneMap.insert(&trackArray[61], &zoneArray[15]);
    zoneMap.insert(&trackArray[62], &zoneArray[18]);
    zoneMap.insert(&trackArray[63], &zoneArray[15]);
    // -------------------- E
    zoneMap.insert(&trackArray[64], &zoneArray[20]);
    zoneMap.insert(&trackArray[65], &zoneArray[23]);
    zoneMap.insert(&trackArray[66], &zoneArray[24]);
    zoneMap.insert(&trackArray[67], &zoneArray[27]);
    zoneMap.insert(&trackArray[68], &zoneArray[28]);
    zoneMap.insert(&trackArray[69], &zoneArray[27]);
    zoneMap.insert(&trackArray[70], &zoneArray[34]);
    zoneMap.insert(&trackArray[71], &zoneArray[33]);
    zoneMap.insert(&trackArray[72], &zoneArray[21]);
    zoneMap.insert(&trackArray[73], &zoneArray[16]);
    zoneMap.insert(&trackArray[74], &zoneArray[11]);
    zoneMap.insert(&trackArray[75], &zoneArray[10]);
    zoneMap.insert(&trackArray[76], &zoneArray[15]);
    zoneMap.insert(&trackArray[77], &zoneArray[16]);
    zoneMap.insert(&trackArray[78], &zoneArray[25]);
    zoneMap.insert(&trackArray[79], &zoneArray[23]);
    // ------  BR1 ---------
    zoneMap.insert(&trackArray[80], &zoneArray[5]);
    zoneMap.insert(&trackArray[81], &zoneArray[5]);
    // ------  2 ---------
    zoneMap.insert(&trackArray[82], &zoneArray[5]);
    zoneMap.insert(&trackArray[83], &zoneArray[5]);
    // ------  3 ---------
    zoneMap.insert(&trackArray[84], &zoneArray[5]);
    zoneMap.insert(&trackArray[85], &zoneArray[5]);
    // ------  4 ---------
    zoneMap.insert(&trackArray[86], &zoneArray[32]);
    zoneMap.insert(&trackArray[87], &zoneArray[32]);
    // ------  5 ---------
    zoneMap.insert(&trackArray[88], &zoneArray[6]);
    zoneMap.insert(&trackArray[89], &zoneArray[6]);
    // ------  6 ---------
    zoneMap.insert(&trackArray[90], &zoneArray[8]);
    zoneMap.insert(&trackArray[91], &zoneArray[8]);
    // ------  7 ---------
    zoneMap.insert(&trackArray[92], &zoneArray[10]);
    zoneMap.insert(&trackArray[93], &zoneArray[10]);
    // ------  8 ---------
    zoneMap.insert(&trackArray[94], &zoneArray[21]);
    zoneMap.insert(&trackArray[95], &zoneArray[21]);
    // ------  9 ---------
    zoneMap.insert(&trackArray[96], &zoneArray[21]);
    zoneMap.insert(&trackArray[97], &zoneArray[21]);
    // ------  10 ---------
    zoneMap.insert(&trackArray[98], &zoneArray[27]);
    zoneMap.insert(&trackArray[99], &zoneArray[27]);
    // ------  11 ---------
    zoneMap.insert(&trackArray[100], &zoneArray[32]);
    zoneMap.insert(&trackArray[101], &zoneArray[32]);
    // ------  12 ---------
    zoneMap.insert(&trackArray[102], &zoneArray[32]);
    zoneMap.insert(&trackArray[103], &zoneArray[32]);
    // ------  13 ---------
    zoneMap.insert(&trackArray[104], &zoneArray[25]);
    zoneMap.insert(&trackArray[105], &zoneArray[25]);
    // ------  14 ---------
    zoneMap.insert(&trackArray[106], &zoneArray[22]);
    zoneMap.insert(&trackArray[107], &zoneArray[22]);
    // ------  15 ---------
    zoneMap.insert(&trackArray[108], &zoneArray[12]);
    zoneMap.insert(&trackArray[109], &zoneArray[12]);
    // ------  16 ---------
    zoneMap.insert(&trackArray[110], &zoneArray[13]);
    zoneMap.insert(&trackArray[111], &zoneArray[13]);
    // ------  17 ---------
    zoneMap.insert(&trackArray[112], &zoneArray[15]);
    zoneMap.insert(&trackArray[113], &zoneArray[15]);
    // ------  18 ---------
    zoneMap.insert(&trackArray[114], &zoneArray[6]);
    zoneMap.insert(&trackArray[115], &zoneArray[6]);
    // ------  153 ---------
    zoneMap.insert(&trackArray[116], &zoneArray[20]);
    zoneMap.insert(&trackArray[117], &zoneArray[20]);
    // ------  154 ---------
    zoneMap.insert(&trackArray[118], &zoneArray[20]);
    zoneMap.insert(&trackArray[119], &zoneArray[20]);
    // ------  155 ---------
    zoneMap.insert(&trackArray[120], &zoneArray[20]);
    zoneMap.insert(&trackArray[121], &zoneArray[20]);
    // ------  156 ---------
    zoneMap.insert(&trackArray[122], &zoneArray[20]);
    zoneMap.insert(&trackArray[123], &zoneArray[20]);
    // ------  EN / EX---------
    // // ------  1 ---------
    zoneMap.insert(&trackArray[124], &zoneArray[20]);
    zoneMap.insert(&trackArray[125], &zoneArray[20]);
    // ------  2 ---------
    zoneMap.insert(&trackArray[126], &zoneArray[20]);
    zoneMap.insert(&trackArray[127], &zoneArray[20]);
    // ------  3 ---------
    zoneMap.insert(&trackArray[128], &zoneArray[7]);
    zoneMap.insert(&trackArray[129], &zoneArray[7]);

    zoneMap.insert(&trackArray[130], &zoneArray[30]);
    zoneMap.insert(&trackArray[131], &zoneArray[30]);
    // ------  5 ---------
    zoneMap.insert(&trackArray[132], &zoneArray[31]);
    zoneMap.insert(&trackArray[133], &zoneArray[31]);

    // ------  7 ---------
    zoneMap.insert(&trackArray[134], &zoneArray[2]);
    zoneMap.insert(&trackArray[135], &zoneArray[2]);

    // ------  9 ---------
    zoneMap.insert(&trackArray[136], &zoneArray[4]);
    zoneMap.insert(&trackArray[137], &zoneArray[4]);
    // ------  10 ---------
    zoneMap.insert(&trackArray[138], &zoneArray[3]);
    zoneMap.insert(&trackArray[139], &zoneArray[3]);
    // ------  4 ---------
    // zoneMap.insert(&trackArray[130], &zoneArray[        ]);
    // zoneMap.insert(&trackArray[131], &zoneArray[        ]);
    // // ------  5 ---------
    // zoneMap.insert(&trackArray[132], &zoneArray[        ]);
    // zoneMap.insert(&trackArray[133], &zoneArray[        ]);

    // // ------  7 ---------
    // zoneMap.insert(&trackArray[134], &zoneArray[        ]);
    // zoneMap.insert(&trackArray[135], &zoneArray[        ]);

    // // ------  9 ---------
    // zoneMap.insert(&trackArray[136], &zoneArray[        ]);
    // zoneMap.insert(&trackArray[137], &zoneArray[        ]);
    // // ------  10 ---------
    // zoneMap.insert(&trackArray[138], &zoneArray[        ]);
    // zoneMap.insert(&trackArray[139], &zoneArray[        ]);
    // ------  FAKE SENSORS ---------
    zoneMap.insert(&trackArray[140], &zoneArray[8]);
    zoneMap.insert(&trackArray[141], &zoneArray[6]);
    zoneMap.insert(&trackArray[142], &zoneArray[6]);
    zoneMap.insert(&trackArray[143], &zoneArray[10]);
    zoneMap.insert(&trackArray[144], &zoneArray[32]);
    zoneMap.insert(&trackArray[145], &zoneArray[22]);
}
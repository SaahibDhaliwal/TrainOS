#ifndef __TURNOUT__
#define __TURNOUT__
#include "cstdint"

#define SINGLE_SWITCH_COUNT 18
#define DOUBLE_SWITCH_COUNT 4

enum class SwitchState { STRAIGHT, CURVED, Unknown };

struct Turnout {
    char id;
    SwitchState state;
};

int turnoutIdx(int turnoutNum);
void initializeTurnouts(Turnout* turnouts, int marklinServerTid, int printerProprietorTid, int clockServerTid);
void initialTurnoutConfigTrackA(Turnout* turnouts);
void initialTurnoutConfigTrackB(Turnout* turnouts);

#endif
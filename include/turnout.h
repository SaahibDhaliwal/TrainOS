#ifndef __TURNOUT__
#define __TURNOUT__
#include "cstdint"

#define SINGLE_SWITCH_COUNT 18
#define DOUBLE_SWITCH_COUNT 4

typedef enum { STRAIGHT = 33, CURVED, UNKNOWN } SwitchState;
typedef struct {
    char id;
    SwitchState state;
} Turnout;

int turnoutIdx(int turnoutNum);
void initializeTurnouts(Turnout* turnouts, int marklinServerTid, int printerProprietorTid, int clockServerTid);
void initialTurnoutConfig(Turnout* turnouts);

#endif
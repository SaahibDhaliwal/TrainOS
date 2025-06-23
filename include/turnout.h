#ifndef __TURNOUT__
#define __TURNOUT__
#include "cstdint"

#define SINGLE_SWITCH_COUNT 18
#define DOUBLE_SWITCH_COUNT 4

typedef enum { STRAIGHT, CURVED, UNKNOWN } SwitchState;

typedef struct {
    char id;
    SwitchState state;
} Turnout;

// void initialize_turnouts(Turnout* turnouts, Queue* marklin);
void print_turnout_table(uint32_t consoleTid);
void update_turnout(Turnout* turnouts, int turnoutNum, uint32_t consoleTid);
int turnoutIdx(int turnoutNum);

#endif
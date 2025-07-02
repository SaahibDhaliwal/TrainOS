#include "turnout.h"

#include "clock_server_protocol.h"
#include "marklin_server_protocol.h"
#include "printer_proprietor_protocol.h"

void initializeTurnouts(Turnout* turnouts, int marklinServerTid, int printerProprietorTid, int clockServerTid) {
    for (int i = 0; i < SINGLE_SWITCH_COUNT; i += 1) {
        turnouts[i].id = i + 1;
        marklin_server::setTurnout(marklinServerTid, turnouts[i].state, i + 1);
        clock_server::Delay(clockServerTid, 20);
        printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(turnouts[i].state), i);
        // also update the track data?
    }

    for (int i = 0; i < DOUBLE_SWITCH_COUNT; i += 1) {
        turnouts[i].id = i + 153;
        if (i == 0 || i == 2) {
            marklin_server::setTurnout(marklinServerTid, turnouts[i].state, i + 153);
            printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(turnouts[i].state),
                                              i + SINGLE_SWITCH_COUNT);
        } else {
            marklin_server::setTurnout(marklinServerTid, turnouts[i].state, i + 153);
            printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(turnouts[i].state),
                                              i + SINGLE_SWITCH_COUNT);
        }
        clock_server::Delay(clockServerTid, 20);
    }
    marklin_server::solenoidOff(marklinServerTid);
}

int turnoutIdx(int turnoutNum) {
    if (turnoutNum >= 1 && turnoutNum <= SINGLE_SWITCH_COUNT) {
        return turnoutNum - 1;
    } else if (turnoutNum >= 153 && turnoutNum <= 153 + DOUBLE_SWITCH_COUNT - 1) {
        return SINGLE_SWITCH_COUNT + (turnoutNum - 153);
    } else {
        return -1;
    }
}

void initialTurnoutConfig(Turnout* turnouts) {
    turnouts[0].state = CURVED;
    turnouts[1].state = CURVED;
    turnouts[2].state = CURVED;
    turnouts[3].state = CURVED;
    turnouts[4].state = CURVED;
    turnouts[5].state = CURVED;
    turnouts[6].state = CURVED;
    turnouts[7].state = CURVED;
    turnouts[8].state = CURVED;
    turnouts[9].state = STRAIGHT;
    turnouts[10].state = CURVED;
    turnouts[11].state = CURVED;
    turnouts[12].state = STRAIGHT;
    turnouts[13].state = CURVED;
    turnouts[14].state = CURVED;
    turnouts[15].state = CURVED;
    turnouts[16].state = CURVED;
    turnouts[17].state = CURVED;
    // double switches
    turnouts[18].state = CURVED;
    turnouts[19].state = STRAIGHT;
    turnouts[20].state = CURVED;
    turnouts[21].state = STRAIGHT;
}

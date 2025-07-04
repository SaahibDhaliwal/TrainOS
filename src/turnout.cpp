#include "turnout.h"

#include "clock_server_protocol.h"
#include "marklin_server_protocol.h"
#include "printer_proprietor_protocol.h"

void initializeTurnouts(Turnout* turnouts, int marklinServerTid, int printerProprietorTid, int clockServerTid) {
    for (int i = 0; i < SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT; i += 1) {
        if (turnouts[i].state == SwitchState::CURVED) {
            marklin_server::setTurnout(marklinServerTid, Command_Byte::SWITCH_CURVED, turnouts[i].id);
            printer_proprietor::updateTurnout(printerProprietorTid, Command_Byte::SWITCH_CURVED, i);
        } else {
            marklin_server::setTurnout(marklinServerTid, Command_Byte::SWITCH_STRAIGHT, turnouts[i].id);
            printer_proprietor::updateTurnout(printerProprietorTid, Command_Byte::SWITCH_STRAIGHT, i);
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
    for (int i = 0; i < SINGLE_SWITCH_COUNT; i += 1) {
        turnouts[i].id = i + 1;
    }

    for (int i = 0; i < DOUBLE_SWITCH_COUNT; i += 1) {
        turnouts[i].id = i + 153;
    }

    turnouts[0].state = SwitchState::CURVED;
    turnouts[1].state = SwitchState::CURVED;
    turnouts[2].state = SwitchState::CURVED;
    turnouts[3].state = SwitchState::CURVED;
    turnouts[4].state = SwitchState::CURVED;
    turnouts[5].state = SwitchState::STRAIGHT;
    turnouts[6].state = SwitchState::STRAIGHT;
    turnouts[7].state = SwitchState::CURVED;
    turnouts[8].state = SwitchState::CURVED;
    turnouts[9].state = SwitchState::STRAIGHT;
    turnouts[10].state = SwitchState::CURVED;
    turnouts[11].state = SwitchState::CURVED;
    turnouts[12].state = SwitchState::STRAIGHT;
    turnouts[13].state = SwitchState::CURVED;
    turnouts[14].state = SwitchState::STRAIGHT;
    turnouts[15].state = SwitchState::CURVED;
    turnouts[16].state = SwitchState::STRAIGHT;
    turnouts[17].state = SwitchState::CURVED;
    // double switches
    turnouts[18].state = SwitchState::CURVED;
    turnouts[19].state = SwitchState::STRAIGHT;
    turnouts[20].state = SwitchState::CURVED;
    turnouts[21].state = SwitchState::STRAIGHT;
}

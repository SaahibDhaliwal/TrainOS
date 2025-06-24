#include "turnout.h"

#include "clock_server_protocol.h"
#include "marklin_server_protocol.h"
#include "printer_proprietor_protocol.h"

void initializeTurnouts(int marklinServerTid, int printerProprietorTid, int clockServerTid) {
    for (int i = 0; i < SINGLE_SWITCH_COUNT; i += 1) {
        marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, i + 1);
        clock_server::Delay(clockServerTid, 20);
        printer_proprietor::updateTurnout(SWITCH_CURVED, i, printerProprietorTid);
    }

    for (int i = 0; i < DOUBLE_SWITCH_COUNT; i += 1) {
        if (i == 0 || i == 2) {
            marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, i + 153);
            printer_proprietor::updateTurnout(SWITCH_CURVED, i + SINGLE_SWITCH_COUNT, printerProprietorTid);

        } else {
            marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, i + 153);
            printer_proprietor::updateTurnout(SWITCH_STRAIGHT, i + SINGLE_SWITCH_COUNT, printerProprietorTid);
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

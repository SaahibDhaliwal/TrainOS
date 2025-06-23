#include "turnout.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "cursor.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "pos.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "test_utils.h"

#define TABLE_START_ROW 15
#define TABLE_START_COL 10

static const Pos turnout_offsets[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT] = {
    {2, 1},  {4, 1},  {6, 1},  {8, 1},  {10, 1}, {12, 1},  {2, 8},   {4, 8},  {6, 8},  {8, 8},  {10, 8},
    {12, 8}, {2, 15}, {4, 15}, {6, 15}, {8, 15}, {10, 15}, {12, 15}, {2, 22}, {4, 22}, {6, 22}, {8, 22}};

void initializeTurnouts(Turnout* turnouts, int marklinServerTid, int printerProprietorTid, int clockServerTid) {
    for (int i = 0; i < SINGLE_SWITCH_COUNT; i += 1) {
        Turnout turnout = {.id = i + 1, .state = CURVED};
        turnouts[i] = turnout;

        marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, turnouts[i].id);
        clock_server::Delay(clockServerTid, 20);
        WITH_HIDDEN_CURSOR(printerProprietorTid, update_turnout(turnouts, i, printerProprietorTid));
    }

    for (int i = 0; i < DOUBLE_SWITCH_COUNT; i += 1) {
        Turnout turnout = {.id = i + 153, .state = UNKNOWN};
        turnouts[i + SINGLE_SWITCH_COUNT] = turnout;

        if (i == 0 || i == 2) {
            marklin_server::setTurnout(marklinServerTid, SWITCH_CURVED, turnouts[i + SINGLE_SWITCH_COUNT].id);
            turnouts[i + SINGLE_SWITCH_COUNT].state = CURVED;
        } else {
            marklin_server::setTurnout(marklinServerTid, SWITCH_STRAIGHT, turnouts[i + SINGLE_SWITCH_COUNT].id);
            turnouts[i + SINGLE_SWITCH_COUNT].state = STRAIGHT;
        }
        clock_server::Delay(clockServerTid, 20);
        WITH_HIDDEN_CURSOR(printerProprietorTid,
                           update_turnout(turnouts, i + SINGLE_SWITCH_COUNT, printerProprietorTid));
    }

    marklin_server::solenoidOff(marklinServerTid);
}

void draw_grid_frame(uint32_t consoleTid) {
    // clang-format off
    const char* lines[] = {
        "           Turnouts          ",
        "┌─────╦──────╦──────╦──────┐",
        "│ 1│  ║  7│  ║ 13│  ║ 153│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 2│  ║  8│  ║ 14│  ║ 154│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 3│  ║  9│  ║ 15│  ║ 155│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 4│  ║ 10│  ║ 16│  ║ 156│ │",
        "├─────╬──────╬──────╬──────┘",
        "│ 5│  ║ 11│  ║ 17│  │",
        "├─────╬──────╬──────│",
        "│ 6│  ║ 12│  ║ 18│  │",
        "└─────╩──────╩──────┘\n"
    };
    // clang-format on

    int num_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < num_lines; i += 1) {
        printer_proprietor::printF(consoleTid, "\033[%d;%dH%s", TABLE_START_ROW + i, TABLE_START_COL, lines[i]);
    }
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

void update_turnout(Turnout* turnouts, int idx, uint32_t printerProprietorTid) {
    Pos pos = turnout_offsets[idx];
    char state = turnouts[idx].state == STRAIGHT ? 'S' : 'C';

    if (pos.col == 22) {
        printer_proprietor::printF(printerProprietorTid, "\033[%d;%dH", TABLE_START_ROW + pos.row,
                                   TABLE_START_COL + pos.col + 4);
    } else {
        printer_proprietor::printF(printerProprietorTid, "\033[%d;%dH", TABLE_START_ROW + pos.row,
                                   TABLE_START_COL + pos.col + 3);
    }

    printer_proprietor::printS(printerProprietorTid, 0, "\033[1m");

    if (turnouts[idx].state == STRAIGHT) {
        cursor_sharp_blue(printerProprietorTid);
    } else {
        cursor_sharp_pink(printerProprietorTid);
    }

    printer_proprietor::printC(printerProprietorTid, 0, state);
    printer_proprietor::printS(printerProprietorTid, 0, "\033[22m");
}

// void fill_turnout_nums(Turnout *turnouts, uint32_t consoleTid) {
//     for (int idx = 0; idx < SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT; idx++) {
//         Pos pos = turnout_offsets[idx];

//         uartPrintf(consoleTid, "\033[%d;%dH", TABLE_START_ROW + pos.row, TABLE_START_COL + pos.col);

//         if (pos.col == 22) {
//             uartPrintf(consoleTid, "%3d│", turnouts[idx].id);
//         } else {
//             uartPrintf(consoleTid, "%2d│", turnouts[idx].id);
//         }
//     }
// }

void print_turnout_table(uint32_t consoleTid) {
    printer_proprietor::printF(consoleTid, "\033[%d;%dH", TABLE_START_ROW, TABLE_START_COL);
    draw_grid_frame(consoleTid);
}

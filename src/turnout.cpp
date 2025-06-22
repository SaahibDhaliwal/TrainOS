#include "turnout.h"

#include "command.h"
#include "cursor.h"
#include "pos.h"
#include "queue.h"
#include "rpi.h"

#define TABLE_START_ROW 15
#define TABLE_START_COL 10

static const Pos turnout_offsets[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT] = {
    {2, 1},  {4, 1},  {6, 1},  {8, 1},  {10, 1}, {12, 1},  {2, 8},   {4, 8},  {6, 8},  {8, 8},  {10, 8},
    {12, 8}, {2, 15}, {4, 15}, {6, 15}, {8, 15}, {10, 15}, {12, 15}, {2, 22}, {4, 22}, {6, 22}, {8, 22}};

// void initialize_turnouts(Turnout *turnouts, Queue *marklin) {
//     for (int i = 0; i < SINGLE_SWITCH_COUNT; i += 1) {
//         Turnout turnout = {.id = i + 1, .state = UNKNOWN};
//         turnouts[i] = turnout;
//         Command turnoutCurvedCmd = Base_Command;
//         turnoutCurvedCmd.address = turnouts[i].id;
//         turnoutCurvedCmd.operation = SWITCH_CURVED;
//         turnoutCurvedCmd.msDelay = 200;

//         queue_push(marklin, turnoutCurvedCmd);
//     }

//     for (int i = 0; i < DOUBLE_SWITCH_COUNT; i += 1) {
//         Turnout turnout = {.id = i + 153, .state = UNKNOWN};
//         turnouts[i + SINGLE_SWITCH_COUNT] = turnout;

//         if (i == 0 || i == 2) {
//             Command turnoutCurvedCmd = Base_Command;
//             turnoutCurvedCmd.address = turnouts[i + SINGLE_SWITCH_COUNT].id;
//             turnoutCurvedCmd.operation = SWITCH_CURVED;

//             queue_push(marklin, turnoutCurvedCmd);
//         } else {
//             Command turnoutStraightCmd = Base_Command;
//             turnoutStraightCmd.address = turnouts[i + SINGLE_SWITCH_COUNT].id;
//             turnoutStraightCmd.operation = SWITCH_STRAIGHT;

//             queue_push(marklin, turnoutStraightCmd);
//         }
//     }

//     Command solenoidOffCmd = Base_Command;
//     solenoidOffCmd.operation = SOLENOID_OFF;
//     solenoidOffCmd.msDelay = 200;

//     queue_push(marklin, solenoidOffCmd);
// }

// void initial_turnout_adjustments(Turnout *turnouts, Queue *marklin) {
//     for (int i = 0; i < SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT; i += 1) {
//         Command turnoutStraightCmd = Base_Command;
//         turnoutStraightCmd.address = turnouts[i].id;
//         turnoutStraightCmd.operation = SWITCH_CURVED;
//         turnoutStraightCmd.msDelay = 200;

//         queue_push(marklin, turnoutStraightCmd);
//     }

//     Command turnoutStraightCmd = Base_Command;
//     turnoutStraightCmd.operation = SWITCH_STRAIGHT;
//     turnoutStraightCmd.msDelay = 200;

//     // 10, 13, 16, 17
//     turnoutStraightCmd.address = 10;
//     queue_push(marklin, turnoutStraightCmd);
//     turnoutStraightCmd.address = 13;
//     queue_push(marklin, turnoutStraightCmd);
//     turnoutStraightCmd.address = 16;
//     queue_push(marklin, turnoutStraightCmd);
//     turnoutStraightCmd.address = 17;
//     queue_push(marklin, turnoutStraightCmd);

//     Command solenoidOffCmd = Base_Command;
//     solenoidOffCmd.operation = SOLENOID_OFF;
//     solenoidOffCmd.msDelay = 200;

//     queue_push(marklin, solenoidOffCmd);
// }

void draw_grid_frame(uint32_t consoleTid) {
    // clang-format off
    const char* lines[] = {
        "           Turnouts          ",
        "┌─────╦──────╦──────╦──────┐",
        "│     ║      ║      ║      │",
        "├─────╬──────╬──────╬──────┤",
        "│     ║      ║      ║      │",
        "├─────╬──────╬──────╬──────┤",
        "│     ║      ║      ║      │",
        "├─────╬──────╬──────╬──────┤",
        "│     ║      ║      ║      │",
        "├─────╬──────╬──────╬──────┘",
        "│     ║      ║      │",
        "├─────╬──────╬──────│",
        "│     ║      ║      │",
        "└─────╩──────╩──────┘\n"
    };
    // clang-format on

    int num_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < num_lines; i += 1) {
        uartPrintf(consoleTid, "\033[%d;%dH%s", TABLE_START_ROW + i, TABLE_START_COL, lines[i]);
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

void update_turnout(Turnout *turnouts, int idx, uint32_t consoleTid) {
    Pos pos = turnout_offsets[idx];
    char state = turnouts[idx].state == STRAIGHT ? 'S' : 'C';

    if (pos.col == 22) {
        uartPrintf(consoleTid, "\033[%d;%dH", TABLE_START_ROW + pos.row, TABLE_START_COL + pos.col + 4);
    } else {
        uartPrintf(consoleTid, "\033[%d;%dH", TABLE_START_ROW + pos.row, TABLE_START_COL + pos.col + 3);
    }

    uartPutS(consoleTid, "\033[1m");

    if (turnouts[idx].state == STRAIGHT) {
        cursor_sharp_blue(consoleTid);
    } else {
        cursor_sharp_pink(consoleTid);
    }

    uartPutC(consoleTid, state);
    uartPutS(consoleTid, "\033[22m");
}

void fill_turnout_nums(Turnout *turnouts, uint32_t consoleTid) {
    for (int idx = 0; idx < SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT; idx++) {
        Pos pos = turnout_offsets[idx];

        uartPrintf(consoleTid, "\033[%d;%dH", TABLE_START_ROW + pos.row, TABLE_START_COL + pos.col);

        if (pos.col == 22) {
            uartPrintf(consoleTid, "%3d│", turnouts[idx].id);
        } else {
            uartPrintf(consoleTid, "%2d│", turnouts[idx].id);
        }
    }
}

void print_turnout_table(Turnout *turnouts, uint32_t consoleTid) {
    uartPrintf(consoleTid, "\033[%d;%dH", TABLE_START_ROW, TABLE_START_COL);
    draw_grid_frame(consoleTid);
    uartPutS(consoleTid, "\033[s");  // save cursor
    fill_turnout_nums(turnouts, consoleTid);
    uartPutS(consoleTid, "\033[u");  // restore cursor
}

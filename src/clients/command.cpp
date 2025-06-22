#include "command.h"

#include <string.h>

#include "cursor.h"
#include "queue.h"
#include "rpi.h"
#include "train.h"

// const Command Base_Command = {.operation = 0, .address = 0, .msDelay = 20, .partialCompletion = false};

#define COMMAND_PROMPT_START_ROW 29
#define COMMAND_PROMPT_START_COL 0

// void process_input_command(char* command, Queue* marklinQueue, Train* trains, uint64_t currentTimeMillis) {
//     if (strncmp(command, "tr ", 3) == 0) {
//         const char* cur = &command[3];

//         if (*cur < '0' || *cur > '9') return;

//         int trainNumber = 0;
//         while (*cur >= '0' && *cur <= '9') {
//             if (trainNumber > 255) return;
//             trainNumber = trainNumber * 10 + (*cur - '0');
//             cur++;
//         }

//         int trainIdx = trainNumToIndex(trainNumber);
//         if (trainIdx == -1) return;

//         if (*cur != ' ') return;
//         cur++;

//         if (*cur < '0' || *cur > '9') return;  // speed
//         int trainSpeed = 0;

//         while (*cur >= '0' && *cur <= '9') {
//             trainSpeed = trainSpeed * 10 + (*cur - '0');
//             cur++;
//         }

//         if (trainSpeed > TRAIN_SPEED_14) return;

//         if (*cur != '\0') return;

//         Command trainSpeedCmd = Base_Command;
//         trainSpeedCmd.operation = (char)(trainSpeed + 16);
//         trainSpeedCmd.address = (uint8_t)trainNumber;
//         queue_push(marklinQueue, trainSpeedCmd);
//         trains[trainIdx].speed = trainSpeed + 16;
//     } else if (strncmp(command, "rv ", 3) == 0) {
//         const char* cur = &command[3];

//         if (*cur < '0' || *cur > '9') return;

//         int trainNumber = 0;
//         while (*cur >= '0' && *cur <= '9') {
//             trainNumber = trainNumber * 10 + (*cur - '0');
//             cur++;
//         }

//         if (*cur != '\0') return;

//         Command stopTrainCmd = Base_Command;
//         stopTrainCmd.address = (char)trainNumber;
//         stopTrainCmd.operation = TRAIN_STOP + 16;

//         queue_push(marklinQueue, stopTrainCmd);

//         int trainIdx = trainNumToIndex(trainNumber);

//         if (!trains[trainIdx].reversing) {
//             trains[trainIdx].reversing = true;
//             trains[trainIdx].reversingStartMillis = currentTimeMillis;
//         }
//     } else if (strncmp(command, "sw ", 3) == 0) {
//         const char* cur = &command[3];

//         if (*cur < '0' || *cur > '9') return;

//         int turnoutNumber = 0;
//         while (*cur >= '0' && *cur <= '9') {
//             turnoutNumber = turnoutNumber * 10 + (*cur - '0');
//             cur++;
//         }

//         if (*cur != ' ') return;
//         cur++;

//         if (*cur != 'S' && *cur != 's' && *cur != 'C' && *cur != 'c') return;  // direction

//         Command_Byte turnoutDirection = SWITCH_STRAIGHT;
//         if (*cur == 'c' || *cur == 'C') turnoutDirection = SWITCH_CURVED;

//         cur++;

//         if (*cur != '\0') return;

//         Command turnoutCmd = Base_Command;
//         turnoutCmd.address = (char)turnoutNumber;
//         turnoutCmd.operation = turnoutDirection;
//         turnoutCmd.msDelay = 200;

//         queue_push(marklinQueue, turnoutCmd);

//         switch (turnoutNumber) {
//             case 153: {
//                 Command turnoutCmd = Base_Command;
//                 turnoutCmd.address = 154;
//                 turnoutCmd.msDelay = 200;
//                 if (turnoutDirection == SWITCH_CURVED) {
//                     turnoutCmd.operation = SWITCH_STRAIGHT;
//                 } else {
//                     turnoutCmd.operation = SWITCH_CURVED;
//                 }

//                 queue_push(marklinQueue, turnoutCmd);
//                 break;
//             }
//             case 154: {
//                 Command turnoutCmd = Base_Command;
//                 turnoutCmd.address = 153;
//                 turnoutCmd.msDelay = 200;
//                 if (turnoutDirection == SWITCH_CURVED) {
//                     turnoutCmd.operation = SWITCH_STRAIGHT;
//                 } else {
//                     turnoutCmd.operation = SWITCH_CURVED;
//                 }
//                 queue_push(marklinQueue, turnoutCmd);

//                 break;
//             }
//             case 155: {
//                 Command turnoutCmd = Base_Command;
//                 turnoutCmd.address = 156;
//                 turnoutCmd.msDelay = 200;
//                 if (turnoutDirection == SWITCH_CURVED) {
//                     turnoutCmd.operation = SWITCH_STRAIGHT;
//                 } else {
//                     turnoutCmd.operation = SWITCH_CURVED;
//                 }
//                 queue_push(marklinQueue, turnoutCmd);

//                 break;
//             }
//             case 156: {
//                 Command turnoutCmd = Base_Command;
//                 turnoutCmd.address = 155;
//                 turnoutCmd.msDelay = 200;
//                 if (turnoutDirection == SWITCH_CURVED) {
//                     turnoutCmd.operation = SWITCH_STRAIGHT;
//                 } else {
//                     turnoutCmd.operation = SWITCH_CURVED;
//                 }
//                 queue_push(marklinQueue, turnoutCmd);

//                 break;
//             }
//         }

//         Command solenoidOffCmd = Base_Command;
//         solenoidOffCmd.operation = SOLENOID_OFF;
//         solenoidOffCmd.msDelay = 200;
//         queue_push(marklinQueue, solenoidOffCmd);
//     }
// }

void print_command_feedback(uint32_t consoleTid, bool validCommand) {
    cursor_down_one(consoleTid);
    clear_current_line(consoleTid);
    if (validCommand) {
        cursor_soft_green(consoleTid);
        uartPutS(consoleTid, "✔ Command accepted");
    } else {
        cursor_soft_red(consoleTid);
        uartPutS(consoleTid, "✖ Invalid command");
    }  // if
}

void print_initial_command_prompt(uint32_t consoleTid) {
    uartPrintf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    uartPutS(consoleTid, "\r> ");
}

void print_clear_command_prompt(uint32_t consoleTid) {
    uartPrintf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    uartPutS(consoleTid, "\r> ");
}

void print_command_prompt_blocked(uint32_t consoleTid) {
    uartPrintf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    uartPutS(consoleTid, "Initializing...");
}

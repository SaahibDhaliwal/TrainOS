#ifndef __COMMAND__
#define __COMMAND__

#include <stdbool.h>
#include <stdint.h>

#include "command_server_protocol.h"

typedef enum {
    TRAIN_STOP = 0x00 + 16,
    TRAIN_SPEED_1,
    TRAIN_SPEED_2,
    TRAIN_SPEED_3,
    TRAIN_SPEED_4,
    TRAIN_SPEED_5,
    TRAIN_SPEED_6,
    TRAIN_SPEED_7,
    TRAIN_SPEED_8,
    TRAIN_SPEED_9,
    TRAIN_SPEED_10,
    TRAIN_SPEED_11,
    TRAIN_SPEED_12,
    TRAIN_SPEED_13,
    TRAIN_SPEED_14,
    TRAIN_REVERSE,
    SOLENOID_OFF = 0x20,
    SWITCH_STRAIGHT,
    SWITCH_CURVED,
    SENSOR_READ_ALL = 0x85,
} Command_Byte;

typedef struct Train Train;

// void process_input_command(char* command, Queue* marklinQueue, Train* trains, uint64_t currentTimeMillis);
void print_command_feedback(uint32_t consoleTid, command_server::Reply reply);
void print_initial_command_prompt(uint32_t consoleTid);
void print_clear_command_prompt(uint32_t consoleTid);
void print_command_prompt_blocked(uint32_t consoleTid);
#endif  // command.h
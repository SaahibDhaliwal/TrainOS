#ifndef __COMMAND__
#define __COMMAND__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TRAIN_STOP = 0x00,
    TRAIN_SPEED_1 = 0x01,
    TRAIN_SPEED_2 = 0x02,
    TRAIN_SPEED_3 = 0x03,
    TRAIN_SPEED_4 = 0x04,
    TRAIN_SPEED_5 = 0x05,
    TRAIN_SPEED_6 = 0x06,
    TRAIN_SPEED_7 = 0x07,
    TRAIN_SPEED_8 = 0x08,
    TRAIN_SPEED_9 = 0x09,
    TRAIN_SPEED_10 = 0x0A,
    TRAIN_SPEED_11 = 0x0B,
    TRAIN_SPEED_12 = 0x0C,
    TRAIN_SPEED_13 = 0x0D,
    TRAIN_SPEED_14 = 0x0E,
    TRAIN_REVERSE = 0x0F,
    SOLENOID_OFF = 0x20,
    SWITCH_STRAIGHT = 0x21,
    SWITCH_CURVED = 0x22,
    SENSOR_READ_ALL = 0x85,
} Command_Byte;

typedef struct Train Train;

// void process_input_command(char* command, Queue* marklinQueue, Train* trains, uint64_t currentTimeMillis);
void print_command_feedback(uint32_t consoleTid, bool validCommand);
void print_initial_command_prompt(uint32_t consoleTid);
void print_clear_command_prompt(uint32_t consoleTid);
void print_command_prompt_blocked(uint32_t consoleTid);
#endif  // command.h
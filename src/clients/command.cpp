#include "command.h"

#include <string.h>

#include "command_server_protocol.h"
#include "cursor.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "train.h"

// const Command Base_Command = {.operation = 0, .address = 0, .msDelay = 20, .partialCompletion = false};

#define COMMAND_PROMPT_START_ROW 29
#define COMMAND_PROMPT_START_COL 0

void print_command_feedback(uint32_t consoleTid, command_server::Reply reply) {
    cursor_down_one(consoleTid);
    clear_current_line(consoleTid);
    if (reply == command_server::Reply::SUCCESS) {
        cursor_soft_green(consoleTid);
        console_server::Puts(consoleTid, 0, "✔ Command accepted");
    } else {
        cursor_soft_red(consoleTid);
        console_server::Puts(consoleTid, 0, "✖ Invalid command");
    }  // if
}

void print_initial_command_prompt(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    console_server::Puts(consoleTid, 0, "\r> ");
}

void print_clear_command_prompt(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    console_server::Puts(consoleTid, 0, "\r> ");
}

void print_command_prompt_blocked(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    console_server::Puts(consoleTid, 0, "Initializing...");
}

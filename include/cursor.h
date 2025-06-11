#ifndef __CURSOR__
#define __CURSOR__

#include <stdbool.h>

#include "rpi.h"

#define WITH_HIDDEN_CURSOR(statement)                      \
    do {                                                   \
        bool wasVisible = get_cursor_visibility();         \
        uart_printf(CONSOLE, "\033[s\033[?25l");           \
        statement;                                         \
        uart_printf(CONSOLE, "\033[u");                    \
        if (wasVisible) uart_printf(CONSOLE, "\033[?25h"); \
    } while (0)

bool get_cursor_visibility();
void backspace();
void clear_current_line();
void clear_screen();
void hide_cursor();
void show_cursor();
void cursor_top_left();
void cursor_down_one();

void cursor_white();
void cursor_soft_blue();
void cursor_soft_pink();
void cursor_soft_green();
void cursor_soft_red();

void cursor_sharp_green();
void cursor_sharp_yellow();
void cursor_sharp_orange();
void cursor_sharp_blue();
void cursor_sharp_pink();

void print_ascii_art();

#endif  // cursor.h
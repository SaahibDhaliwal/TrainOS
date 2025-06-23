#ifndef __CURSOR__
#define __CURSOR__

#include <stdbool.h>

#include "printer_proprietor_protocol.h"
#include "rpi.h"

// blocking

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

bool get_cursor_visibility();

// not blocking

#define WITH_HIDDEN_CURSOR(console, statement)                            \
    do {                                                                  \
        bool wasVisible = get_cursor_visibility();                        \
        printer_proprietor::printF(console, "\033[s\033[?25l");           \
        statement;                                                        \
        printer_proprietor::printF(console, "\033[u");                    \
        if (wasVisible) printer_proprietor::printF(console, "\033[?25h"); \
    } while (0)

void backspace(int console);
void clear_current_line(int console);
void clear_screen(int console);
void hide_cursor(int console);
void show_cursor(int console);
void cursor_top_left(int console);
void cursor_down_one(int console);

void cursor_white(int console);
void cursor_soft_blue(int console);
void cursor_soft_pink(int console);
void cursor_soft_green(int console);
void cursor_soft_red(int console);

void cursor_sharp_green(int console);
void cursor_sharp_yellow(int console);
void cursor_sharp_orange(int console);
void cursor_sharp_blue(int console);
void cursor_sharp_pink(int console);

void print_ascii_art(int console);

#endif  // cursor.h
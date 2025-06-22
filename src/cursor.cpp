#include "cursor.h"

#include "rpi.h"

static bool g_isCursorVisible = false;

bool get_cursor_visibility() {
    return g_isCursorVisible;
}

void backspace() {
    uart_putc(CONSOLE, '\b');
    uart_putc(CONSOLE, ' ');
    uart_putc(CONSOLE, '\b');
}

void cursor_down_one() {
    uart_puts(CONSOLE, "\033[1B");
}

void clear_current_line() {
    uart_puts(CONSOLE, "\033[2K");
}

void clear_screen() {
    uart_puts(CONSOLE, "\033[2J");
}

void hide_cursor() {
    uart_puts(CONSOLE, "\033[?25l");
    g_isCursorVisible = false;
}

void show_cursor() {
    uart_puts(CONSOLE, "\033[?25h");
    g_isCursorVisible = true;
}

void cursor_top_left() {
    uart_puts(CONSOLE, "\033[H");
}

void cursor_white() {
    uart_puts(CONSOLE, "\033[0m");
}

void cursor_soft_blue() {
    uart_puts(CONSOLE, "\033[38;5;153m");
}

void cursor_soft_pink() {
    uart_puts(CONSOLE, "\033[38;5;218m");
}

void cursor_soft_green() {
    uart_puts(CONSOLE, "\033[38;5;34m");
}

void cursor_soft_red() {
    uart_puts(CONSOLE, "\033[38;5;160m");
}

void cursor_sharp_green() {
    uart_puts(CONSOLE, "\033[38;5;46m");
}

void cursor_sharp_yellow() {
    uart_puts(CONSOLE, "\033[38;5;226m");
}

void cursor_sharp_orange() {
    uart_puts(CONSOLE, "\033[38;5;202m");
}

void cursor_sharp_blue() {
    uart_puts(CONSOLE, "\033[38;5;45m");
}

void cursor_sharp_pink() {
    uart_puts(CONSOLE, "\033[38;5;201m");
}

void print_ascii_art() {
    cursor_soft_pink();
    // clang-format off
	uart_puts(CONSOLE,
	"__| |________________________________________________________________________________| |__\r\n"
	"__   ________________________________________________________________________________   __\r\n"
	"  | |                _____                        _                                  | |  \r\n"
	"  | |               |_   _|     _ _    __ _      (_)     _ _       ___               | |  \r\n"
	"  | |                 | |      | '_|  / _` |     | |    | ' \\     (_-<               | |  \r\n"
	"  | |                _|_|_    _|_|_   \\__,_|    _|_|_   |_||_|    /__/_              | |  \r\n"
	"  | |              _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"|              | | \r\n"
	"  | |              \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-'             | |  \r\n"
	"__| |________________________________________________________________________________| |__\r\n"
	"__   ________________________________________________________________________________   __\r\n"
	"  | |                                                                                | |  \r\n"
	);
    // clang-format on
}

void backspace(int console) {
    uartPutS(console, "\b \b");
}

void cursor_down_one(int console) {
    uartPutS(console, "\033[1B");
}

void clear_current_line(int console) {
    uartPutS(console, "\033[2K");
}

void clear_screen(int console) {
    uartPutS(console, "\033[2J");
}

void hide_cursor(int console) {
    uartPutS(console, "\033[?25l");
    g_isCursorVisible = false;
}

void show_cursor(int console) {
    uartPutS(console, "\033[?25h");
    g_isCursorVisible = true;
}

void cursor_top_left(int console) {
    uartPutS(console, "\033[H");
}

void cursor_white(int console) {
    uartPutS(console, "\033[0m");
}

void cursor_soft_blue(int console) {
    uartPutS(console, "\033[38;5;153m");
}

void cursor_soft_pink(int console) {
    uartPutS(console, "\033[38;5;218m");
}

void cursor_soft_green(int console) {
    uartPutS(console, "\033[38;5;34m");
}

void cursor_soft_red(int console) {
    uartPutS(console, "\033[38;5;160m");
}

void cursor_sharp_green(int console) {
    uartPutS(console, "\033[38;5;46m");
}

void cursor_sharp_yellow(int console) {
    uartPutS(console, "\033[38;5;226m");
}

void cursor_sharp_orange(int console) {
    uartPutS(console, "\033[38;5;202m");
}

void cursor_sharp_blue(int console) {
    uartPutS(console, "\033[38;5;45m");
}

void cursor_sharp_pink(int console) {
    uartPutS(console, "\033[38;5;201m");
}

void print_ascii_art(int console) {
    cursor_soft_pink(console);
    // clang-format off
	uartPutS(console,
	"__| |________________________________________________________________________________| |__\n\r"
	"__   ________________________________________________________________________________   __\n\r"
	"  | |                _____                        _                                  | |  \n\r"
	"  | |               |_   _|     _ _    __ _      (_)     _ _       ___               | |  \n\r"
	"  | |                 | |      | '_|  / _` |     | |    | ' \\     (_-<               | |  \n\r"
	"  | |                _|_|_    _|_|_   \\__,_|    _|_|_   |_||_|    /__/_              | |  \n\r"
	"  | |              _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"|              | | \n\r"
	"  | |              \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-'             | |  \n\r"
	"__| |________________________________________________________________________________| |__\n\r"
	"__   ________________________________________________________________________________   __\n\r"
	"  | |                                                                                | |  \n\r"
	);
    // clang-format on
}
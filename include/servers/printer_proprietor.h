#ifndef __PRINTER_SERVER__
#define __PRINTER_SERVER__

#define WITH_HIDDEN_CURSOR(console, statement)                        \
    do {                                                              \
        bool wasVisible = get_cursor_visibility();                    \
        console_server::Printf(console, "\033[s\033[?25l");           \
        statement;                                                    \
        console_server::Printf(console, "\033[u");                    \
        if (wasVisible) console_server::Printf(console, "\033[?25h"); \
    } while (0)

void PrinterProprietor();

#endif
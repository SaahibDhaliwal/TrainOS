#ifndef __TEST_UTILS__
#define __TEST_UTILS__

#include "rpi.h"

#define ASSERT(expr, ...)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            uart_puts(CONSOLE, "\033[38;5;160m");                                                  \
            uart_printf(CONSOLE, "[%s][FAIL] %s:%d: %s\r\n", __func__, __FILE__, __LINE__, #expr); \
            uart_printf(CONSOLE, "" __VA_ARGS__);                                                  \
            uart_puts(CONSOLE, "\033[0m");                                                         \
            for (;;) {                                                                             \
                __asm__ volatile("wfi");                                                           \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                            \
    do {                                                        \
        uart_printf(CONSOLE, "[%s] Starting %s\r\n", #fn, #fn); \
        fn();                                                   \
        uart_puts(CONSOLE, "\033[38;5;34m");                    \
        uart_printf(CONSOLE, "[%s] %s PASSED!\r\n", #fn, #fn);  \
        uart_puts(CONSOLE, "\033[0m");                          \
    } while (0)

#endif
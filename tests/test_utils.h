#ifndef __TEST_UTILS__
#define __TEST_UTILS__

#include "cursor.h"

#define ASSERT(expr, ...)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            cursor_soft_red();                                                                     \
            uart_printf(CONSOLE, "[%s][FAIL] %s:%d: %s\r\n", __func__, __FILE__, __LINE__, #expr); \
            uart_printf(CONSOLE, "" __VA_ARGS__);                                                  \
            cursor_white();                                                                        \
            for (;;) {                                                                             \
                __asm__ volatile("wfi");                                                           \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                            \
    do {                                                        \
        uart_printf(CONSOLE, "[%s] Starting %s\r\n", #fn, #fn); \
        fn();                                                   \
        cursor_soft_green();                                    \
        uart_printf(CONSOLE, "[%s] %s PASSED!\r\n", #fn, #fn);  \
        cursor_white();                                         \
    } while (0)

#endif
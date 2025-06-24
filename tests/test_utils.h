#ifndef __TEST_UTILS__
#define __TEST_UTILS__
#include "rpi.h"
#define ASSERT(expr, ...)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            uart_printf(CONSOLE, "[%s][FAIL] %s:%d: %s\r\n", __func__, __FILE__, __LINE__, #expr); \
            uart_printf(CONSOLE, "" __VA_ARGS__);                                                  \
            for (;;) {                                                                             \
                __asm__ volatile("wfi");                                                           \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                            \
    do {                                                        \
        uart_printf(CONSOLE, "[%s] Starting %s\r\n", #fn, #fn); \
        fn();                                                   \
        uart_printf(CONSOLE, "[%s] %s PASSED!\r\n", #fn, #fn);  \
    } while (0)

#endif
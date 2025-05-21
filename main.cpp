#include <string.h>

#include <array>

#include "rpi.h"
#include "task_manager.h"

extern "C" void setup_mmu();

void run_init_array() {
    using init_func_t = void (*)();
    extern init_func_t __init_array_start[];
    extern init_func_t __init_array_end[];

    for (init_func_t* func = __init_array_start; func < __init_array_end; func += 1) {
        if (*func) {
            (*func)();
        }
    }
}

void initialize_bss() {
    extern unsigned int __bss_start__;
    extern unsigned int __bss_end__;

    for (unsigned int* p = &__bss_start__; p < &__bss_end__; p += 1) {
        *p = 0;
    }
}

int main() {
#if defined(MMU)
    setup_mmu();
#endif
    initialize_bss();  // sets entire bss region to 0's
    run_init_array();  // call constructors


    // set up GPIO pins for both console and marklin uarts
    gpio_init();
    // not strictly necessary, since console is configured during boot
    uart_config_and_enable(CONSOLE);
    // welcome message
    uart_puts(CONSOLE, "\r\nHello world, this is version: " __DATE__ " / " __TIME__ "\r\n\r\nPress 'q' to reboot\r\n");

    unsigned int counter = 1;

    TaskManager taskManager;

    //   initialize();
    //   for (;;) {
    //     currtask = schedule();
    //     request = activate(currtask); // activate does eret
    //     handle(request);
    //   }
    // initialize user task: set up stack and "fake" context and resume

    for (;;) {
        uart_printf(CONSOLE, "PI[%u]> ", counter++);
        for (;;) {
            char c = uart_getc(CONSOLE);
            uart_putc(CONSOLE, c);
            if (c == '\r') {
                uart_putc(CONSOLE, '\n');
                break;
            } else if (c == 'q' || c == 'Q') {
                uart_puts(CONSOLE, "\r\n");
                return 0;
            }
        }
    }
}

#if !defined(MMU)
#include <stddef.h>

// define our own memset to avoid SIMD instructions emitted from the compiler
void* memset(void* s, int c, size_t n) {
    for (char* it = (char*)s; n > 0; --n) *it++ = c;
    return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
void* memcpy(void* dest, const void* src, size_t n) {
    char* sit = (char*)src;
    char* cdest = (char*)dest;
    for (size_t i = 0; i < n; ++i) *cdest++ = *sit++;
    return dest;
}
#endif

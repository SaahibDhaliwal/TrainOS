#include <stdint.h>

#include "rpi.h"

void dummy_handler() {
    uint64_t regs[30];  // should this be larger?
    asm volatile(
        "mov x16, sp\n"
        "stp x0, x1, [%0]\n"
        "stp x2, x3, [%0, #16]\n"
        "stp x4, x5, [%0, #32]\n"
        "stp x6, x7, [%0, #48]\n"
        "stp x8, x9, [%0, #64]\n"
        "stp x10, x11, [%0, #80]\n"
        "stp x12, x13, [%0, #96]\n"
        "stp x14, x15, [%0, #112]\n"
        "stp x16, x17, [%0, #128]\n"
        "stp x18, x19, [%0, #144]\n"
        "stp x20, x21, [%0, #160]\n"
        "stp x22, x23, [%0, #176]\n"
        "stp x24, x25, [%0, #192]\n"
        "stp x26, x27, [%0, #208]\n"
        "stp x28, x29, [%0, #224]\n"
        "str x30, [%0, #240]\n"
        :
        : "r"(regs)
        : "x16", "memory"); //clobber list

    for (int i = 0; i < 31; i++) {
        // note that this truncates to the lower 32 bits
        uart_printf(CONSOLE, "x%u = %x\n\r", i, regs[i]);
    }
}
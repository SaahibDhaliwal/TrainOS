#include <stdint.h>

#include <cstdlib>

#include "rpi.h"

extern "C" void dummy_handler() {
    uint64_t esr_el1_val;
    asm volatile("mrs %[x], esr_el1" : [x] "=r"(esr_el1_val));
    uart_printf(CONSOLE, "INVALID EXCEPTION VECTOR ENTRY. ESR_EL1: 0x%x\n\r", esr_el1_val);

    while (true) {
        asm volatile("wfe");
    }
}

extern "C" void test_handler() {
    uart_printf(CONSOLE, "YOU MESSED SOMETHING UP IN THE KERNEL ASSEMBLY!\n\r");
    uint64_t esr_el1_val;
    asm volatile("mrs %[x], esr_el1" : [x] "=r"(esr_el1_val));
    uart_printf(CONSOLE, "ESR_EL1: 0x%x\n\r", esr_el1_val);
    while (true) {
        asm volatile("wfe");
    }
}
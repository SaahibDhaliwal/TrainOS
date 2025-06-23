#include <stdint.h>

#include <cstdlib>

#include "rpi.h"
#include "test_utils.h"

extern "C" void dummy_handler() {
    uint64_t esr_el1_val;
    asm volatile("mrs %[x], esr_el1" : [x] "=r"(esr_el1_val));
    ASSERT(0, "INVALID EXCEPTION VECTOR ENTRY. ESR_EL1: 0x%x\r\n", esr_el1_val);

    while (true) {
        asm volatile("wfe");
    }
}

extern "C" void test_handler() {
    uint64_t esr_el1_val;
    asm volatile("mrs %[x], esr_el1" : [x] "=r"(esr_el1_val));
    ASSERT(0, "YOU MESSED SOMETHING UP IN THE KERNEL ASSEMBLY!\r\n ESR_EL1: 0x%x\r\n", esr_el1_val);
    while (true) {
        asm volatile("wfe");
    }
}
#include "rpi.h"
#include <stdint.h>
#include "task.h"

void dummy_handler(){
    uint64_t regs[30]; //should this be larger?
    asm volatile (
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
        : "r" (regs)
        : "x16", "x17", "memory"
    );

    for (int i = 0; i < 31; i++) {
        // note that this truncates to the lower 32 bits
        uart_printf(CONSOLE, "x%u = %x\n", i, (unsigned int)regs[i]); 
    }

}


uint64_t getRegister(int reg_num) {
    switch (reg_num) {
        case 1: return getRegisterx1();
        case 2: return getRegisterx2();
        case 3: return getRegisterx3();
        case 4: return getRegisterx4();
        case 5: return getRegisterx5();
        case 6: return getRegisterx6();
        case 7: return getRegisterx7();
        case 8: return getRegisterx8();
        case 9: return getRegisterx9();
        case 10: return getRegisterx10();
        case 11: return getRegisterx11();
        case 12: return getRegisterx12();
        case 13: return getRegisterx13();
        case 14: return getRegisterx14();
        case 15: return getRegisterx15();
        case 16: return getRegisterx16();
        case 17: return getRegisterx17();
        case 18: return getRegisterx18();
        case 19: return getRegisterx19();
        case 20: return getRegisterx20();
        case 21: return getRegisterx21();
        case 22: return getRegisterx22();
        case 23: return getRegisterx23();
        case 24: return getRegisterx24();
        case 25: return getRegisterx25();
        case 26: return getRegisterx26();
        case 27: return getRegisterx27();
        case 28: return getRegisterx28();
        case 29: return getRegisterx29();
        case 30: return getRegisterx30();
        default: return 0;
    }
}

void setRegister(int reg_num, uint64_t value) {
    switch (reg_num) {
        case 1: setRegisterx1(value); break;
        case 2: setRegisterx2(value); break;
        case 3: setRegisterx3(value); break;
        case 4: setRegisterx4(value); break;
        case 5: setRegisterx5(value); break;
        case 6: setRegisterx6(value); break;
        case 7: setRegisterx7(value); break;
        case 8: setRegisterx8(value); break;
        case 9: setRegisterx9(value); break;
        case 10: setRegisterx10(value); break;
        case 11: setRegisterx11(value); break;
        case 12: setRegisterx12(value); break;
        case 13: setRegisterx13(value); break;
        case 14: setRegisterx14(value); break;
        case 15: setRegisterx15(value); break;
        case 16: setRegisterx16(value); break;
        case 17: setRegisterx17(value); break;
        case 18: setRegisterx18(value); break;
        case 19: setRegisterx19(value); break;
        case 20: setRegisterx20(value); break;
        case 21: setRegisterx21(value); break;
        case 22: setRegisterx22(value); break;
        case 23: setRegisterx23(value); break;
        case 24: setRegisterx24(value); break;
        case 25: setRegisterx25(value); break;
        case 26: setRegisterx26(value); break;
        case 27: setRegisterx27(value); break;
        case 28: setRegisterx28(value); break;
        case 29: setRegisterx29(value); break;
        case 30: setRegisterx30(value); break;
        default: break;
    }
}


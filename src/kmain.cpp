#include <array>

#include "rpi.h"
#include "rps_server.h"
#include "sys_call_handler.h"
#include "task_manager.h"
#include "user_tasks.h"

extern "C" {
void setup_mmu();
void vbar_init();
}

// calls constructors
void run_init_array() {
    typedef void (*funcvoid0_t)();
    extern funcvoid0_t __init_array_start, __init_array_end;  // defined in linker script
    for (funcvoid0_t* ctr = &__init_array_start; ctr < &__init_array_end; ctr += 1) {
        (*ctr)();
    }  // for
}

// zeros out the bss
void initialize_bss() {
    extern unsigned int __bss_start__, __bss_end__;  // defined in linker script
    for (unsigned int* p = &__bss_start__; p < &__bss_end__; p += 1) {
        *p = 0;
    }  // for
}

extern "C" int kmain() {
#if defined(MMU)
    setup_mmu();
#endif
    initialize_bss();                 // sets entire bss region to 0's
    run_init_array();                 // call constructors
    vbar_init();                      // sets up exception vector
    gpio_init();                      // set up GPIO pins for both console and marklin uarts
    uart_config_and_enable(CONSOLE);  // not strictly necessary, since console is configured during boot

    TaskDescriptor* curTask = nullptr;  // the current user task
    TaskManager taskManager;            // interface for task scheduling and creation
    SysCallHandler sysCallHandler;      // interface for handling system calls, extracts/returns params

    taskManager.createTask(nullptr, 0, reinterpret_cast<uint64_t>(IdleTask));          // idle task
    taskManager.createTask(nullptr, 4, reinterpret_cast<uint64_t>(RPSFirstUserTask));  // spawn parent task
    for (;;) {
        curTask = taskManager.schedule();
        if (!curTask) break;
        uint32_t request = taskManager.activate(curTask);
        sysCallHandler.handle(request, &taskManager, curTask);
    }  // for
    return 0;
}  // kmain

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

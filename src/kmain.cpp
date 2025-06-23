#include <array>

#include "gic.h"
#include "idle_time.h"
#include "kernel_utils.h"
#include "rpi.h"
#include "servers/clock_server.h"
#include "servers/console_server.h"
#include "servers/printer_proprietor.h"
#include "servers/rps_server.h"
#include "sys_call_handler.h"
#include "task_manager.h"
#include "test_entry.h"
#include "user_tasks.h"

extern "C" {
void setup_mmu();
void vbar_init();
}

extern "C" int kmain() {
#if defined(MMU)
    setup_mmu();
#endif
    kernel_util::initialize_bss();  // sets entire bss region to 0's
    kernel_util::run_init_array();  // call constructors
    vbar_init();                    // sets up exception vector
    gpio_init();                    // set up GPIO pins for both console and marklin uarts

    uart_config_and_enable(CONSOLE);  // not strictly necessary, since console is configured during boot
    uart_config_and_enable(MARKLIN);

#if defined(TESTING)
    uart_getc(CONSOLE);
    runTests();

#else
    TaskDescriptor* curTask = nullptr;  // the current user task
    TaskManager taskManager;            // interface for task scheduling and creation
    bool quitFlag = false;

    taskManager.createTask(nullptr, 0, reinterpret_cast<uint64_t>(IdleTask));             // idle task
    taskManager.createTask(nullptr, 10, reinterpret_cast<uint64_t>(FinalFirstUserTask));  // spawn parent task
    for (;;) {
        curTask = taskManager.schedule();
        // in the event there are no more tasks to schedule or if we call quit
        if (!curTask || quitFlag) {
            break;
        }
        uint32_t request = taskManager.activate(curTask);
        kernel_util::handle(request, &taskManager, curTask, &quitFlag);
    }  // for

#endif  // TESTING

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

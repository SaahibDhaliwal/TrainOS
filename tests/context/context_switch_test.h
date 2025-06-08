#include "context.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"

extern "C" {
void fillRegisters(uint64_t base);
void dumpRegisters(uint64_t buf[34]);
}

void taskEntry() {
    uint64_t buf[34];

    fillRegisters(200);  // modifies everything except for link register so we actually return
    sys::Yield();        // user to kernel

    dumpRegisters(buf);  // x0 will be different, everything else should be the same as before we left
    for (uint64_t i = 19; i < 29; i += 1) {
        TEST_ASSERT(buf[i] == (200 + i));
    }

    sys::Exit();
}

void runContextSwitchTest() {
    TaskManager taskManager;
    TaskDescriptor* curTask = nullptr;
    uint64_t buf[34];

    taskManager.createTask(nullptr, 1, reinterpret_cast<uint64_t>(taskEntry));  // test tasK
    curTask = taskManager.schedule();

    Context kernelContext = taskManager.getKernelContext();

    fillRegisters(100);  // modifies everything except for link register so we actually return
    uint32_t ESR_EL1 = kernelToUser(&kernelContext, curTask->getMutableContext());

    dumpRegisters(buf);
    // x0 will be different, everything else should be the same as before we left
    for (uint64_t i = 19; i < 29; ++i) {
        TEST_ASSERT(buf[i] == (100 + i));
    }
}

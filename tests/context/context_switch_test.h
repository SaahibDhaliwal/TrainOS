#include "context.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"

extern "C" {
void fillCallerSavedRegisters(uint64_t base);
void fillCalleeSavedRegisters(uint64_t base);
void dumpRegisters(uint64_t buf[34]);
}

constexpr int kernelRegValue = 1;
constexpr int taskRegValue = 2;

void taskEntry() {
    uint64_t buf[34];

    fillCallerSavedRegisters(taskRegValue);  // modifies everything except for link register so we actually return
    // fillCalleeSavedRegisters(taskRegValue);
    sys::Yield();  // user to kernel context switch

    dumpRegisters(buf);  // x0 is the buf, everything else should be the same as what we filled
    for (uint64_t i = 1; i <= 34; ++i) {
        ASSERT(buf[i] == (taskRegValue + i));
    }

    sys::Exit();
}

void runContextSwitchTest() {
    TaskManager taskManager;
    TaskDescriptor* curTask = nullptr;
    uint64_t buf[34];

    taskManager.createTask(nullptr, 1, reinterpret_cast<uint64_t>(taskEntry));  // test task
    curTask = taskManager.schedule();

    Context kernelContext = taskManager.getKernelContext();

    fillCallerSavedRegisters(kernelRegValue);  // modifies everything except for link register so we actually return
    // fillCalleeSavedRegisters(kernelRegValue);

    slowKernelToUser(&kernelContext, curTask->getMutableContext());  // kernel to user context switch

    dumpRegisters(buf);  // x0 is the buf, everything else should be the same as what we filled
    for (uint64_t i = 1; i < 29; i++) {
        // if (!(buf[i] == (kernelRegValue + i))) {
        //     uart_printf(CONSOLE, "Expr I value: %u\r\n", i);
        // }
        ASSERT(buf[i] == (kernelRegValue + i), "i = %u buf[i] = %u kernelRegValue + i = %u", i, buf[i],
               kernelRegValue + i);
    }

    taskManager.rescheduleTask(curTask);
    curTask = taskManager.schedule();
    kernelContext = taskManager.getKernelContext();
    fillCallerSavedRegisters(kernelRegValue);
    slowKernelToUser(&kernelContext, curTask->getMutableContext());  // kernel to user context switch
}

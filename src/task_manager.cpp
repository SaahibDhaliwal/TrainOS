#include "task_manager.h"

#include <stdint.h>

#include <algorithm>

#include "config.h"
#include "gic.h"
#include "interrupt.h"
#include "rpi.h"
#include "task_descriptor.h"
#include "test_utils.h"
#include "timer.h"

TaskManager::TaskManager() : nextTaskId(0), clockEventTask(nullptr) {
    for (int i = 0; i < Config::MAX_TASKS; i += 1) {
        taskSlabs[i].setTid(i);
        freelist.push(&taskSlabs[i]);
    }  // for
}

uint32_t TaskManager::createTask(TaskDescriptor* parent, uint64_t priority, uint64_t entryPoint) {
    if (freelist.empty()) return -1;
    if (priority >= Config::MAX_PRIORITY) return -1;

    TaskDescriptor* td = freelist.pop();
    uint64_t stackTop = Config::USER_STACK_BASE - (td->getTid() * Config::USER_TASK_STACK_SIZE);
    td->initialize(parent, priority, entryPoint, stackTop);
    readyQueues[priority].push(td);

    return td->getTid();
}

void TaskManager::exitTask(TaskDescriptor* task) {
    task->setState(TaskState::EXITED);
    freelist.push(task);
}

void TaskManager::rescheduleTask(TaskDescriptor* task) {
    uint8_t priority = task->getPriority();
    readyQueues[priority].push(task);
    task->setState(TaskState::READY);
}

int TaskManager::awaitEvent(int64_t eventId, TaskDescriptor* task) {
    if (eventId == static_cast<int64_t>(INTERRUPT_NUM::CLOCK)) {
        ASSERT(clockEventTask == nullptr);  // no other task should be waiting on clock

        task->setState(TaskState::WAITING_FOR_EVENT);
        clockEventTask = task;
        return 0;
    } else {  // invalid event
        return -1;
    }
}

void TaskManager::handleInterrupt(int64_t eventId) {
    if (eventId == static_cast<int64_t>(INTERRUPT_NUM::CLOCK)) {
        timerSetNextTick();
        // uart_printf(CONSOLE, "interrupt ID: %u\n\r", interrupt_id);

        gicEndInterrupt(eventId);
        // uart_printf(CONSOLE, "Current time: %u\n\r", timerGet());

        rescheduleTask(clockEventTask);
        clockEventTask = nullptr;
    }
}

bool TaskManager::isTidValid(int64_t tid) {
    return tid >= 0 && tid < Config::MAX_TASKS && taskSlabs[tid].getState() != TaskState::EXITED;
}

TaskDescriptor* TaskManager::getTask(uint32_t tid) {
    return &taskSlabs[tid];
}

Context TaskManager::getKernelContext() {
    return kernelContext;
}

TaskDescriptor* TaskManager::schedule() {
    for (int priority = Config::MAX_PRIORITY - 1; priority >= 0; priority -= 1) {
        if (!readyQueues[priority].empty()) {
            TaskDescriptor* task = readyQueues[priority].pop();
            task->setState(TaskState::ACTIVE);
            return task;
        }  // if
    }  // for

    return nullptr;
}

uint32_t TaskManager::activate(TaskDescriptor* task) {
    // Kernel execution will pause here and resume when the task traps back into the kernel.
    // ESR_EL1 value is returned when switching from user to kernel.
    uint32_t ESR_EL1 = slowKernelToUser(&kernelContext, task->getMutableContext());
    return ESR_EL1 & 0xFFFFFF;
}

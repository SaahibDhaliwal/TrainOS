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

TaskManager::TaskManager()
    : nextTaskId(0),
      clockEventTask(nullptr),
      consoleTXEventTask(nullptr),
      consoleRXEventTask(nullptr),
      marklinTXEventTask(nullptr),
      marklinRXEventTask(nullptr),
      marklinCTSEventTask(nullptr),
      nonIdleTime(0),
      totalNonIdleTime(0),
      idleTimePercentage(0) {
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
    if (eventId == static_cast<int64_t>(EVENT_ID::CLOCK)) {
        ASSERT(clockEventTask == nullptr);  // no other task should be waiting on clock
        task->setState(TaskState::WAITING_FOR_EVENT);
        clockEventTask = task;
        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::CONSOLE_TX)) {
        ASSERT(consoleTXEventTask == nullptr);  // no other task should be waiting on the console
        task->setState(TaskState::WAITING_FOR_EVENT);
        consoleTXEventTask = task;

        uartSetIMSC(CONSOLE, IMSC::TX);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::CONSOLE_RXRT)) {
        ASSERT(consoleRXEventTask == nullptr);  // no other task should be waiting on the console
        task->setState(TaskState::WAITING_FOR_EVENT);
        consoleRXEventTask = task;

        uartSetIMSC(CONSOLE, IMSC::RX);
        uartSetIMSC(CONSOLE, IMSC::RT);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::MARKLIN_TX)) {
        ASSERT(marklinTXEventTask == nullptr);  // no other task should be waiting on the marklin
        task->setState(TaskState::WAITING_FOR_EVENT);
        marklinTXEventTask = task;
        uartSetIMSC(CONSOLE, IMSC::TX);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::MARKLIN_RX)) {
        ASSERT(marklinRXEventTask == nullptr);  // no other task should be waiting on the marklin
        task->setState(TaskState::WAITING_FOR_EVENT);
        marklinRXEventTask = task;

        uartSetIMSC(CONSOLE, IMSC::RX);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::MARKLIN_CTS)) {
        ASSERT(marklinCTSEventTask == nullptr);  // no other task should be waiting on the marklin
        task->setState(TaskState::WAITING_FOR_EVENT);
        marklinCTSEventTask = task;

        uartSetIMSC(CONSOLE, IMSC::CTS);

        return 0;
    } else {  // invalid event
        return -1;
    }
}

void TaskManager::handleInterrupt(int64_t eventId) {
    if (eventId == static_cast<int64_t>(INTERRUPT_NUM::CLOCK)) {
        timerSetNextTick();
        gicEndInterrupt(eventId);
        rescheduleTask(clockEventTask);
        // since we push it on the priority queue, it's ok to set this back to null?
        // so it will leave it's allocated space in the slab? No
        clockEventTask = nullptr;
    } else if (eventId == static_cast<int64_t>(INTERRUPT_NUM::UART)) {
        // read uart MIS to find out which interrupt(s)

        // right now, it will only clear interrupts one at a time.
        // So if we have multiple in one, we only disable and clear once interrupt per UART. So it will fire again.

        // BUT! what if our notifier that we reschedule does not run in time?
        // if we have TX, and then reschedule the listener, and then we interrupt again for RX, we can't reschedule the
        // same task since it is on the queue and the pointer is empty
        // do we need a buffer of items if we have
        int mis = uartCheckMIS(CONSOLE);
        // uart_printf(CONSOLE, "MIS value: %d\n\r", mis);

        // FOR CONSOLE
        if (mis) {
            int txCompare = static_cast<int>(MIS::TX);
            int rxCompare = static_cast<int>(MIS::RX);
            int rtCompare = static_cast<int>(MIS::RT);

            if (mis >= rtCompare || (mis >= rxCompare && mis < txCompare)) {
                // disable interrupt at IMSC
                uartClearIMSC(CONSOLE, IMSC::RT);
                // clear interrupt with UART ICR
                uartClearICR(CONSOLE, IMSC::RT);

                if ((mis >= rxCompare && mis < txCompare)) {  // in the event they are both on?
                    uartClearIMSC(CONSOLE, IMSC::RX);
                    uartClearICR(CONSOLE, IMSC::RX);
                }

                consoleRXEventTask->setReturnValue(mis);
                rescheduleTask(consoleRXEventTask);
                consoleRXEventTask = nullptr;

            } else if (mis >= txCompare && mis <= rtCompare) {
                uartClearIMSC(CONSOLE, IMSC::TX);
                uartClearICR(CONSOLE, IMSC::TX);

                consoleTXEventTask->setReturnValue(mis);
                rescheduleTask(consoleTXEventTask);
                consoleTXEventTask = nullptr;

            } else if (mis >= rxCompare && mis < txCompare && consoleRXEventTask != nullptr) {
                uartClearIMSC(CONSOLE, IMSC::RX);
                uartClearICR(CONSOLE, IMSC::RX);

                consoleRXEventTask->setReturnValue(mis);
                rescheduleTask(consoleRXEventTask);
                consoleRXEventTask = nullptr;
            } else {
                // something broke
                uart_printf(CONSOLE, "MIS Console Check broke! \n\r");
            }
        }

        // FOR MARKLIN
        mis = uartCheckMIS(MARKLIN);
        if (mis) {
            if (mis >= static_cast<int>(MIS::TX)) {
                uartClearIMSC(MARKLIN, IMSC::TX);
                uartClearICR(MARKLIN, IMSC::TX);
            } else if (mis >= static_cast<int>(MIS::RX)) {
                uartClearIMSC(MARKLIN, IMSC::RX);
                uartClearICR(MARKLIN, IMSC::RX);
            } else if (mis == static_cast<int>(MIS::CTS)) {
                uartClearIMSC(MARKLIN, IMSC::CTS);
                uartClearICR(MARKLIN, IMSC::CTS);
            } else {
                // something broke
                uart_printf(CONSOLE, "MIS Marklin Check broke! \n\r");
            }
            marklinEventTask->setReturnValue(mis);
            rescheduleTask(marklinEventTask);
        }

        // write to GICC_EOIR when finished in kernel...?
        gicEndInterrupt(eventId);
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

uint64_t TaskManager::getIdle() {
    return idleTimePercentage;
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
    // ESR_EL1 or interrupt code is returned when switching from user to kernel.

    if (task->getTid() == 0 &&
        nonIdleTime != 0) {  // ensuring we don't get here on the first time we idle. Will this still be needed?
        uint64_t currTime = timerGetRelativeTime();
        totalNonIdleTime += currTime - nonIdleTime;  // will be some number of ticks
        idleTimePercentage = ((currTime - totalNonIdleTime) * 10000) / currTime;

        // task->setReturnValue(idleTimePercentage);  // send how much time we have not been idling since our last idle
        //  uart_printf(CONSOLE, "STARTING IDLE TASK \n\r");
    }

    uint32_t request = slowKernelToUser(&kernelContext, task->getMutableContext());

    if (task->getTid() == 0) {
        nonIdleTime = timerGetRelativeTime();  // track the timestamp we left our idle task
    }
    return request & 0xFFFFFF;
}

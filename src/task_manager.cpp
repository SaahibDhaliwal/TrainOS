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
        uartSetIMSC(MARKLIN, IMSC::TX);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::MARKLIN_RX)) {
        ASSERT(marklinRXEventTask == nullptr);  // no other task should be waiting on the marklin
        task->setState(TaskState::WAITING_FOR_EVENT);
        marklinRXEventTask = task;

        uartSetIMSC(MARKLIN, IMSC::RX);

        return 0;
    } else if (eventId == static_cast<int64_t>(EVENT_ID::MARKLIN_CTS)) {
        ASSERT(marklinCTSEventTask == nullptr);  // no other task should be waiting on the marklin
        task->setState(TaskState::WAITING_FOR_EVENT);
        marklinCTSEventTask = task;

        uartSetIMSC(MARKLIN, IMSC::CTS);

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
        clockEventTask = nullptr;
    } else if (eventId == static_cast<int64_t>(INTERRUPT_NUM::UART)) {
        int mis = uartCheckMIS(CONSOLE);

        // FOR CONSOLE
        if (mis) {
            int txCompare = static_cast<int>(MIS::TX);
            int rxCompare = static_cast<int>(MIS::RX);
            int rtCompare = static_cast<int>(MIS::RT);
            int misOriginal = mis;

            if (mis > 57 || mis < 8) {
                // something broke
                uart_printf(CONSOLE, "MIS Console Check broke! \n\r");
            }

            if ((mis & (rxCompare | rtCompare)) != 0) {
                uartClearIMSC(CONSOLE, IMSC::RX);
                uartClearICR(CONSOLE, IMSC::RX);
                uartClearIMSC(CONSOLE, IMSC::RT);
                uartClearICR(CONSOLE, IMSC::RT);
                rescheduleTask(consoleRXEventTask);
                consoleRXEventTask = nullptr;
            }

            if (mis & txCompare) {
                uartClearIMSC(CONSOLE, IMSC::TX);
                uartClearICR(CONSOLE, IMSC::TX);
                rescheduleTask(consoleTXEventTask);
                consoleTXEventTask = nullptr;
            }

            // if (mis >= rtCompare) {
            //     // disable interrupt at IMSC
            //     uartClearIMSC(CONSOLE, IMSC::RT);
            //     // clear interrupt with UART ICR
            //     uartClearICR(CONSOLE, IMSC::RT);
            //     mis -= rtCompare;
            //     rescheduleTask(consoleRXEventTask);
            //     consoleRXEventTask = nullptr;
            // }
            // if (mis >= txCompare) {
            //     uartClearIMSC(CONSOLE, IMSC::TX);
            //     uartClearICR(CONSOLE, IMSC::TX);
            //     mis -= txCompare;
            //     rescheduleTask(consoleTXEventTask);
            //     consoleTXEventTask = nullptr;
            // }
            // if (mis == rxCompare && consoleRXEventTask != nullptr) {
            //     uartClearIMSC(CONSOLE, IMSC::RX);
            //     uartClearICR(CONSOLE, IMSC::RX);
            //     // so that if we already got an RT interrupt, we don't need to reschedule the RX task
            //     // since it's already been done
            //     // but we DO want to clear the interrupt at IMSC if it's high
            //     if (consoleRXEventTask != nullptr) {
            //         rescheduleTask(consoleRXEventTask);
            //         consoleRXEventTask = nullptr;
            //     }
            // }
        }

        // FOR MARKLIN
        mis = uartCheckMIS(MARKLIN);
        if (mis) {
            int txCompare = static_cast<int>(MIS::TX);
            int rxCompare = static_cast<int>(MIS::RX);
            int ctsCompare = static_cast<int>(MIS::CTS);
            int misOriginal = mis;

            if (mis > 25 || mis <= 0) {
                // something broke
                uart_printf(CONSOLE, "MIS Console Check broke! \n\r");
            }

            if (mis >= static_cast<int>(MIS::TX)) {
                uartClearIMSC(MARKLIN, IMSC::TX);
                uartClearICR(MARKLIN, IMSC::TX);
                mis -= txCompare;
                marklinTXEventTask->setReturnValue(misOriginal);
                rescheduleTask(marklinTXEventTask);
                marklinTXEventTask = nullptr;
            }
            if (mis >= static_cast<int>(MIS::RX)) {
                uartClearIMSC(MARKLIN, IMSC::RX);
                uartClearICR(MARKLIN, IMSC::RX);
                mis -= rxCompare;
                marklinRXEventTask->setReturnValue(misOriginal);
                rescheduleTask(marklinRXEventTask);
                marklinRXEventTask = nullptr;
            }
            if (mis == static_cast<int>(MIS::CTS)) {
                uartClearIMSC(MARKLIN, IMSC::CTS);
                uartClearICR(MARKLIN, IMSC::CTS);
                marklinCTSEventTask->setReturnValue(mis);  // this is not original
                rescheduleTask(marklinCTSEventTask);
                marklinCTSEventTask = nullptr;
            }
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

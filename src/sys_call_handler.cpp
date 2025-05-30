#include "sys_call_handler.h"

#include "rpi.h"
#include "task_descriptor.h"
#include "task_manager.h"

void SysCallHandler::handle(uint32_t N, TaskManager* taskManager, TaskDescriptor* curTask) {
    switch (static_cast<SYSCALL_NUM>(N)) {
        case SYSCALL_NUM::CREATE: {
            uint64_t priority = curTask->getReg(0);
            uint64_t entryPoint = curTask->getReg(1);

            int tid = taskManager->createTask(curTask, priority, entryPoint);
            curTask->setReturnValue(tid);
            taskManager->rescheduleTask(curTask);
            break;
        }
        case SYSCALL_NUM::MY_TID: {
            uint64_t tid = curTask->getTid();

            curTask->setReturnValue(tid);
            taskManager->rescheduleTask(curTask);
            break;
        }
        case SYSCALL_NUM::MY_PARENT_TID: {
            uint64_t pid = curTask->getPid();

            curTask->setReturnValue(pid);
            taskManager->rescheduleTask(curTask);
            break;
        }
        case SYSCALL_NUM::YIELD: {
            taskManager->rescheduleTask(curTask);
            break;
        }
        case SYSCALL_NUM::EXIT: {
            taskManager->exitTask(curTask);
            break;
        }
        case SYSCALL_NUM::SEND: {
           // taskManager->exitTask(curTask);
            break;
        }
        case SYSCALL_NUM::RECEIVE: {
           // taskManager->exitTask(curTask);
            break;
        }
        case SYSCALL_NUM::REPLY: {
            //taskManager->exitTask(curTask);
            break;
        }
        default: {  // we can make this more extensive
            uart_printf(CONSOLE, "Unknown syscall: %u\n", N);
            break;
        }
    }  // switch
}
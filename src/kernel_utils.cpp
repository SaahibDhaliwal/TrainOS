#include "kernel_utils.h"

#include <algorithm>

#include "gic.h"
#include "rpi.h"
#include "sys_call_handler.h"
#include "task_manager.h"

typedef void (*funcvoid0_t)();
extern funcvoid0_t __init_array_start, __init_array_end;  // defined in linker script
extern unsigned int __bss_start__, __bss_end__;           // defined in linker script

namespace kernel_util {

// calls constructors
void run_init_array() {
    for (funcvoid0_t* ctr = &__init_array_start; ctr < &__init_array_end; ctr += 1) {
        (*ctr)();
    }  // for
}

// zeros out the bss
void initialize_bss() {
    for (unsigned int* p = &__bss_start__; p < &__bss_end__; p += 1) {
        *p = 0;
    }  // for
}

void handle(uint32_t N, TaskManager* taskManager, TaskDescriptor* curTask) {
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
            int64_t receiverTid = curTask->getReg(0);

            if (!taskManager->isTidValid(receiverTid)) {
                curTask->setReturnValue(-1);
                break;
            }

            TaskDescriptor* receiver = taskManager->getTask(receiverTid);

            if (receiver->getState() == TaskState::WAITING_FOR_SEND) {
                const char* senderMsg = reinterpret_cast<const char*>(curTask->getReg(1));
                size_t senderMsgLen = static_cast<size_t>(curTask->getReg(2));

                uint32_t* receiverSenderOut = reinterpret_cast<uint32_t*>(receiver->getReg(0));
                char* receiverMsg = reinterpret_cast<char*>(receiver->getReg(1));
                size_t receiverMsgLen = static_cast<size_t>(receiver->getReg(2));

                size_t n = std::min(senderMsgLen, receiverMsgLen);
                for (size_t i = 0; i < n; ++i) {
                    receiverMsg[i] = senderMsg[i];
                }

                *receiverSenderOut = curTask->getTid();  // set sender tid for receiver

                receiver->setReturnValue(n);
                taskManager->rescheduleTask(receiver);

                curTask->setState(TaskState::WAITING_FOR_REPLY);
            } else {
                // our intended receiver isn't receiving yet, so we get blocked and queue on their sender
                curTask->setState(TaskState::WAITING_FOR_RECEIVE);
                receiver->enqueueSender(curTask);
            }

            break;
        }
        case SYSCALL_NUM::RECEIVE: {
            uint32_t* receiverSenderOut = reinterpret_cast<uint32_t*>(curTask->getReg(0));
            char* receiverMsg = reinterpret_cast<char*>(curTask->getReg(1));
            size_t receiverMsgLen = static_cast<size_t>(curTask->getReg(2));

            if (!curTask->hasSenders()) {                        // do we have any senders?
                curTask->setState(TaskState::WAITING_FOR_SEND);  // block the receiver
                break;
            }

            TaskDescriptor* sender = curTask->dequeueSender();

            if (sender->getState() == TaskState::WAITING_FOR_RECEIVE) {
                char* senderMsg = reinterpret_cast<char*>(sender->getReg(1));
                size_t senderMsgLen = static_cast<size_t>(sender->getReg(2));

                size_t n = std::min(senderMsgLen, receiverMsgLen);
                for (size_t i = 0; i < n; i += 1) {
                    receiverMsg[i] = senderMsg[i];
                }

                *receiverSenderOut = sender->getTid();  // set sender tid for receiver

                curTask->setReturnValue(n);
                taskManager->rescheduleTask(curTask);

                sender->setState(TaskState::WAITING_FOR_REPLY);

            } else {  // send-receive-reply could not be completed
                sender->setReturnValue(-2);
                taskManager->rescheduleTask(sender);
                break;
            }
            break;
        }
        case SYSCALL_NUM::REPLY: {
            int64_t senderTid = curTask->getReg(0);

            if (!taskManager->isTidValid(senderTid)) {
                curTask->setReturnValue(-1);
                taskManager->rescheduleTask(curTask);
                break;
            }

            TaskDescriptor* sender = taskManager->getTask(senderTid);

            if (sender->getState() == TaskState::WAITING_FOR_REPLY) {
                const char* replierMsg = reinterpret_cast<const char*>(curTask->getReg(1));
                size_t replierMsgLen = static_cast<size_t>(curTask->getReg(2));

                char* senderReplyMsg = reinterpret_cast<char*>(sender->getReg(3));
                size_t senderReplyMsgLen = static_cast<size_t>(sender->getReg(4));

                size_t n = std::min(replierMsgLen, senderReplyMsgLen);
                for (size_t i = 0; i < n; i += 1) {
                    senderReplyMsg[i] = replierMsg[i];
                }

                sender->setReturnValue(n);
                taskManager->rescheduleTask(sender);

                curTask->setReturnValue(n);
                taskManager->rescheduleTask(curTask);
            } else {
                curTask->setReturnValue(-2);
                taskManager->rescheduleTask(curTask);
            }

            break;
        }
        case SYSCALL_NUM::AWAIT_EVENT: {
            int64_t eventId = curTask->getReg(0);
            int ret = taskManager->awaitEvent(eventId, curTask);
            if (ret == -1) {  // invalid event?
                curTask->setReturnValue(-1);
                taskManager->rescheduleTask(curTask);
            }

            break;
        }
        case SYSCALL_NUM::INTERRUPT: {
            // read output compare register and add offset for next timer tick
            uint32_t interruptId = gicGetInterrupt();

            taskManager->handleInterrupt(interruptId);

            taskManager->rescheduleTask(curTask);

            break;
        }
        default: {  // we can make this more extensive
            uart_printf(CONSOLE, "Unknown syscall: %u\n", N);
            break;
        }
    }  // switch
}

}  // namespace kernel_util
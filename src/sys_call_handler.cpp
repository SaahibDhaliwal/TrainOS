#include "sys_call_handler.h"

#include <algorithm>

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
            int64_t receiverTid = curTask->getReg(0);

            if (!taskManager->isTidValid(receiverTid)) {
                curTask->setReturnValue(-1);
                break;
            }

            const char* senderMsg = reinterpret_cast<const char*>(curTask->getReg(1));
            size_t senderMsgLen = static_cast<size_t>(curTask->getReg(2));

            TaskDescriptor* receiver = taskManager->getTask(receiverTid);

            if (receiver->getState() == TaskState::WAITING_FOR_SEND) {
                uint32_t* receiverSenderOut = reinterpret_cast<uint32_t*>(receiver->getReg(0));
                char* receiverMsg = reinterpret_cast<char*>(receiver->getReg(1));
                size_t receiverMsgLen = static_cast<size_t>(receiver->getReg(2));

                // set sender tid for receiver
                *receiverSenderOut = curTask->getTid();

                // copy message
                size_t n = std::min(senderMsgLen, receiverMsgLen);
                for (size_t i = 0; i < n; ++i) {
                    receiverMsg[i] = senderMsg[i];
                }

                curTask->setState(TaskState::WAITING_FOR_REPLY);

                receiver->setReturnValue(n);
                taskManager->rescheduleTask(receiver);

            } else {
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
            // reciever has a list of senders

            // if the person in that list is not actually waiting for a a recievie
            TaskDescriptor* sender = curTask->dequeueSender();
            if (sender->getState() == TaskState::WAITING_FOR_RECEIVE) {
                sender->setState(TaskState::WAITING_FOR_REPLY);

                char* senderMsg = reinterpret_cast<char*>(sender->getReg(1));
                size_t senderMsgLen = static_cast<size_t>(sender->getReg(2));

                // set sender tid for receiver
                *receiverSenderOut = sender->getTid();

                // copy message
                size_t n = std::min(senderMsgLen, receiverMsgLen);
                for (size_t i = 0; i < n; ++i) {
                    receiverMsg[i] = senderMsg[i];
                }

                curTask->setReturnValue(n);
                taskManager->rescheduleTask(curTask);
            } else {  // send-receive-reply could not be completed
                sender->setReturnValue(-2);
                taskManager->rescheduleTask(sender);
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

            if (sender->getState() != TaskState::WAITING_FOR_REPLY) {
                curTask->setReturnValue(-2);
                taskManager->rescheduleTask(curTask);
                break;
            }

            const char* replierMsg = reinterpret_cast<const char*>(curTask->getReg(1));
            size_t replierMsgLen = static_cast<size_t>(curTask->getReg(2));

            char* senderReplyMsg = reinterpret_cast<char*>(sender->getReg(3));
            size_t senderReplyMsgLen = static_cast<size_t>(sender->getReg(4));

            // copy message
            size_t n = std::min(replierMsgLen, senderReplyMsgLen);
            for (size_t i = 0; i < n; i += 1) {
                senderReplyMsg[i] = replierMsg[i];
            }

            // uart_printf(CONSOLE, "message 'replier' is sending to og 'sender': %s\n\r", senderReplyMsg);

            // name server had already called reciever
            // rps server calls name server -> sees name server is ready to recieve
            // rps server copies message to name server
            // name server returns does shit
            // name server sends back a reply
            // we copied the reply here

            sender->setReturnValue(n);
            taskManager->rescheduleTask(sender);

            curTask->setReturnValue(n);
            taskManager->rescheduleTask(curTask);
            break;
        }
        default: {  // we can make this more extensive
            uart_printf(CONSOLE, "Unknown syscall: %u\n", N);
            break;
        }
    }  // switch
}
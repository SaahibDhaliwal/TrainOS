#ifndef __SYS_CALL_HANDLER__
#define __SYS_CALL_HANDLER__

#include <cstdint>

enum class SYSCALL_NUM { CREATE, MY_TID, MY_PARENT_TID, YIELD, EXIT, SEND, RECEIVE, REPLY };

class TaskManager;
class TaskDescriptor;

class SysCallHandler {
   public:
    void handle(uint32_t N, TaskManager* taskManager, TaskDescriptor* curTask);
};

#endif /* sys_call_handler.h */
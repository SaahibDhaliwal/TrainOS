#ifndef __TASK_MANAGER__
#define __TASK_MANAGER__

#include "config.h"
#include "context.h"
#include "queue.h"
#include "stack.h"
#include "task_descriptor.h"

// Notes:
// alignas(16) char stackSlabs[Config::USER_TASK_STACK_SIZE * Config::MAX_TASKS];
//
// By doing this, we are placing the user stacks on the kernel stack. This is fine.
// But when you actually start using these stack in EL0
// and you use a local variable or anything, things will break.
// This is because in the MMU, this memory can only acted upon in EL1.
// So by trying to R/W this memory while still in EL0, a data abort happens and is handled
// in our exception vector.
// To avoid these issues we start allocating user stacks at the end of user space memory
// at a 16 byte aligned address.
// See Config::USER_STACK_BASE.

class TaskManager {
   private:
    uint64_t nextTaskId;             // tid of next task
    Stack<TaskDescriptor> freelist;  // free list of TaskDescriptor's
    Context kernelContext;  // kernel's contex
    TaskDescriptor taskSlabs[Config::MAX_TASKS];              // slabs of TaskDescriptor's
    Queue<TaskDescriptor> readyQueues[Config::MAX_PRIORITY];  // intrusive scheduling queue

   public:
    TaskManager();
    int createTask(TaskDescriptor* parent, uint64_t priority, uint64_t entryPoint);  // creates user task
    void exitTask(TaskDescriptor* task);                                             // exit user task
    void rescheduleTask(TaskDescriptor* task);                                       // adds task back to ready queue
    TaskDescriptor* schedule();
    uint32_t activate(TaskDescriptor* task);
};

#endif /* task_manager.h */
#ifndef _taskcreation_h
#define _taskcreation_h_ 1

// Allocates and initializes a task descriptor, using the given priority,
// and the given function pointer as a pointer to the entry point of executable code.

class TaskDescriptor {
      public:
    TaskDescriptor() {};
};

// When Create returns, the task descriptor has all the state needed to run the task, the task's stack has been suitably
// initialized, and the task has been entered into its ready queue so that it will run the next time it is scheduled.
int Create(int priority, void (*function)());

// Returns the task id of the calling task.
int MyTid();

// Returns the task id of the task that created the calling task
int MyParentTid();

// Causes a task to pause executing. The task is moved to the end of its priority queue, and will resume executing when
// next scheduled.
void Yield();

// Causes a task to cease execution permanently. It is removed from all priority queues, send queues, receive queues and
// event queues Resources owned by the task, primarily its memory and task descriptor, may be reclaimed.
void Exit();

#endif /* taskcreation.h */
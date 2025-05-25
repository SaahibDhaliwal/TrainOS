
#ifndef __SYS_CALL_STUBS__
#define __SYS_CALL_STUBS__

extern "C" {
int Create(int priority, void (*function)());  // Creates task and queues it. Caller becomes parent.

int MyTid();  // Returns the task ID of the calling task.

int MyParentTid();  // Returns the task ID of the calling task's creator.

void Yield();  // Pauses calling task's execution, placing the task at the end of its priority queue.

void Exit();  // Terminates calling task and removes it from all queues. Slab is returned to free list.
}

#endif /*sys_call_stubs.h*/
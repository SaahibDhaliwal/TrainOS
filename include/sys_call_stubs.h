#ifndef __SYS_CALL_STUBS__
#define __SYS_CALL_STUBS__
#include <string.h>

#include <cstdint>

#include "util.h"

namespace sys {

extern "C" {
int Create(int priority, void (*function)());  // Creates task and queues it. Caller becomes parent.

int MyTid();  // Returns the task ID of the calling task.

int MyParentTid();  // Returns the task ID of the calling task's creator.

void Yield();  // Pauses calling task's execution, placing the task at the end of its priority queue.

void Exit();  // Terminates calling task and removes it from all queues. Slab is returned to free list.

int Send(uint32_t tid, const char *msg, int msglen, char *reply, int rplen);

int Receive(uint32_t *tid, char *msg, int msglen);

int Reply(uint32_t tid, const char *reply, int rplen);

int AwaitEvent(int eventId);
}
}  // namespace sys

#endif /*sys_call_stubs.h*/
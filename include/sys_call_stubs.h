#ifndef __SYS_CALL_STUBS__
#define __SYS_CALL_STUBS__
#include <string.h>

extern "C" {
int Create(int priority, void (*function)());  // Creates task and queues it. Caller becomes parent.

int MyTid();  // Returns the task ID of the calling task.

int MyParentTid();  // Returns the task ID of the calling task's creator.

void Yield();  // Pauses calling task's execution, placing the task at the end of its priority queue.

void Exit();  // Terminates calling task and removes it from all queues. Slab is returned to free list.

int Send(uint64_t tid, const char *msg, int msglen, char *reply, int rplen);

int Receive(uint64_t *tid, char *msg, int msglen);

int Reply(uint64_t tid, const char *reply, int rplen);
}

int RegisterAs(const char *name){
    //send to name server
    char reply[] = "";
    // replace zero with the actual name server task ID if it's changed
    // UPDATE: this won't actually be zero since the name server gets initialized in the FirstUserTask (as required from the assignment)
    int result = Send(0, name, strlen(name), reply, strlen(reply));
    if (result < 0){
        return -1;
    }
    return 0;
}

int WhoIs(const char *name){
    //send to name server
    char reply[] = "";
    // CHANGE THIS TOO V
    int result = Send(0, name, strlen(name), reply, strlen(reply));
    if (result < 0){
        return -1;
    } 
    // could also check if it returns with error if no task was registered
    return result;
}

#endif /*sys_call_stubs.h*/
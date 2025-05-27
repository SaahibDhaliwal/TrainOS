#ifndef __TASK_DESCRIPTOR__
#define __TASK_DESCRIPTOR__

#include <cstdint>

#include "context.h"

enum class TaskState { UNDEFINED, ACTIVE, READY, EXITED, SEND_BLOCKED, RECEIVE_BLOCKED, REPLY_BLOCKED, EVENT_BLOCKED };

class TaskDescriptor {
    Context context;            // task context
    uint64_t tid;               // task identifier
    uint64_t slabIdx;           // idx in pre-allocated TaskDescriptor slabs
    TaskDescriptor* parent;     // parent pointer
    uint8_t priority;           // priority
    TaskDescriptor* ready_tid;  // unused
    TaskDescriptor* send_tid;   // unused
    TaskState state;            // current run state
    TaskDescriptor* next;       // intrusive link

    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

   public:
    void initialize(uint64_t tid, TaskDescriptor* parent, uint8_t priority, uint64_t entryPoint, uint64_t stackTop);

    // getters
    uint64_t getTid() const;
    uint64_t getPid() const;
    uint64_t getReg(uint8_t reg) const;
    uint64_t getSlabIdx() const;
    uint8_t getPriority() const;
    Context* getMutableContext();  // needs to be manipulated on contextswitch.S

    // setters
    void setReturnValue(uint64_t val);
    void setSlabIdx(uint64_t idx);
    void setState(TaskState state);
};

#endif /* task_descriptor.h */
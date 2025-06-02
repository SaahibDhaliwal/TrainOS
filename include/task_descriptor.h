#ifndef __TASK_DESCRIPTOR__
#define __TASK_DESCRIPTOR__

#include <cstdint>

#include "context.h"
#include "queue.h"

enum class TaskState {
    UNDEFINED,
    ACTIVE,
    READY,
    EXITED,
    WAITING_FOR_SEND,
    WAITING_FOR_RECEIVE,
    WAITING_FOR_REPLY,
    EVENT_BLOCKED
};

class TaskDescriptor {
    Context context;                // task context
    int32_t tid;                    // task identifier
    TaskDescriptor* parent;         // parent pointer
    uint8_t priority;               // priority
    Queue<TaskDescriptor> senders;  // who wants to send to us?
    TaskState state;                // current run state
    TaskDescriptor* next;           // intrusive link

    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

   public:
    void initialize(TaskDescriptor* parent, uint8_t priority, uint64_t entryPoint, uint64_t stackTop);

    // getters
    int32_t getTid() const;
    int32_t getPid() const;
    int64_t getReg(uint8_t reg) const;
    uint8_t getPriority() const;
    Context* getMutableContext();  // needs to be manipulated on contextswitch.S
    TaskState getState() const;
    bool hasSenders() const;

    // setters
    void setReturnValue(uint64_t val);
    void setTid(int32_t tid);
    void setState(TaskState state);

    void enqueueSender(TaskDescriptor* sender) {
        // the below line is redundant?
        // right now, even after queueing the sender, we manually change the state
        // ex: see where this gets called
        sender->setState(TaskState::WAITING_FOR_RECEIVE);
        senders.push(sender);
    }

    TaskDescriptor* dequeueSender() {
        return senders.pop();
    }
};

#endif /* task_descriptor.h */
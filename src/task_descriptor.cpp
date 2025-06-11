#include "task_descriptor.h"

#include "queue.h"
#include "rpi.h"

void TaskDescriptor::initialize(TaskDescriptor* newParent, uint8_t newPriority, uint64_t newEntryPoint,
                                uint64_t newStackTop) {
    for (int i = 0; i < 31; i += 1) {
        context.registers[i] = 0;
    }
    context.sp = newStackTop;
    context.elr = newEntryPoint;
    context.spsr = 0;

    parent = newParent;
    priority = newPriority;
    senders.reset();
    state = TaskState::READY;
}

/*********** GETTERS ********************************/
int32_t TaskDescriptor::getPid() const {
    if (parent) {
        return parent->getTid();
    }
    return -1;
}

int32_t TaskDescriptor::getTid() const {
    return tid;
}

int64_t TaskDescriptor::getReg(uint8_t reg) const {
    if (reg >= 31) {
        uart_printf(CONSOLE, "Trying to get register %d!\r\n", reg);
        return 0;
    }

    return context.registers[reg];
}

uint8_t TaskDescriptor::getPriority() const {
    return priority;
}

Context* TaskDescriptor::getMutableContext() {
    return &context;
}

TaskState TaskDescriptor::getState() const {
    return state;
}

bool TaskDescriptor::hasSenders() const {
    return !senders.empty();
}

/*********** SETTERS ********************************/
void TaskDescriptor::setReturnValue(uint64_t val) {
    context.registers[0] = val;
}

void TaskDescriptor::setTid(int32_t newTid) {
    tid = newTid;
}

void TaskDescriptor::setState(TaskState newState) {
    state = newState;
}

void TaskDescriptor::enqueueSender(TaskDescriptor* sender) {
    senders.push(sender);
}

TaskDescriptor* TaskDescriptor::dequeueSender() {
    return senders.pop();
}

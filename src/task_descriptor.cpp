#include "task_descriptor.h"

#include "rpi.h"

void TaskDescriptor::initialize(uint64_t newTid, TaskDescriptor* newParent, uint8_t newPriority, uint64_t newEntryPoint,
                                uint64_t newStackTop) {
    for (int i = 0; i < 31; i += 1) {
        context.registers[i] = 0;
    }
    context.sp = newStackTop;
    context.elr = newEntryPoint;
    context.spsr = 0;

    tid = newTid;
    parent = newParent;
    priority = newPriority;
    ready_tid = nullptr;
    send_tid = nullptr;
    state = TaskState::READY;
    next = nullptr;
}

/*********** GETTERS ********************************/
uint64_t TaskDescriptor::getPid() const {
    if (parent) {
        return parent->getTid();
    }
    return -1;
}

uint64_t TaskDescriptor::getTid() const {
    return tid;
}

uint64_t TaskDescriptor::getReg(uint8_t reg) const {
    if (reg >= 31) {
        uart_printf(CONSOLE, "Trying to get register %d!\n\r", reg);
        return 0;
    }

    return context.registers[reg];
}

uint64_t TaskDescriptor::getSlabIdx() const {
    return slabIdx;
}

uint8_t TaskDescriptor::getPriority() const {
    return priority;
}

Context* TaskDescriptor::getMutableContext() {
    return &context;
}

/*********** SETTERS ********************************/
void TaskDescriptor::setReturnValue(uint64_t val) {
    context.registers[0] = val;
}

void TaskDescriptor::setSlabIdx(uint64_t idx) {
    slabIdx = idx;
}

void TaskDescriptor::setState(TaskState newState) {
    state = newState;
}

#include "task_descriptor.h"

uint8_t TaskDescriptor::next_tid = 0;

TaskDescriptor::TaskDescriptor() : tid(next_tid++), pid(0) {
}
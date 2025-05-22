#include "task_descriptor.h"

uint8_t TaskDescriptor::next_tid = 0;

TaskDescriptor::TaskDescriptor() : tid(next_tid++), pid(0) {
}

uint64_t TaskDescriptor::GetPid(){
    if(pid != 0){
        return pid->GetTid();
    }
    return -1;
}

uint64_t TaskDescriptor::GetTid(){
    return tid;
}
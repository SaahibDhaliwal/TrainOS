#ifndef __KERNEL_UTILS__
#define __KERNEL_UTILS__

#include <stdint.h>

class TaskManager;
class TaskDescriptor;

namespace kernel_util {
// calls constructors
void run_init_array();

// zeroes out the bss
void initialize_bss();

// handles traps into the kernel
void handle(uint32_t N, TaskManager* taskManager, TaskDescriptor* curTask, bool* quitFlag);

}  // namespace kernel_util

#endif
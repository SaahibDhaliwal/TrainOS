#define MAX_TASKS 64
#define USER_TASK_STACK_SIZE 1024 * 4
#include "queue.h"

class TaskDescriptor;

class TaskManager {
   private:
    char* slabs[USER_TASK_STACK_SIZE * MAX_TASKS];

   public:
    TaskManager();
};
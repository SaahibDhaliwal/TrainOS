#define MAX_TASKS 64
#define USER_TASK_STACK_SIZE 1024 * 4
#include "queue.h"
#include "task_descriptor.h"


// This will always run at the kernel level. Manages everything
class TaskManager {
  private:
    char* slabs[USER_TASK_STACK_SIZE * MAX_TASKS];
    TaskDescriptor* current_task;

    //placeholder for now, but should probably live here regardless
    Queue<TaskDescriptor*, MAX_TASKS/4 > priority_1;
    Queue<TaskDescriptor*, MAX_TASKS/4 > priority_2;
    Queue<TaskDescriptor*, MAX_TASKS/4 > priority_3;
    Queue<TaskDescriptor*, MAX_TASKS/4 > priority_4;

    void handle_request(int error_code);
    char* get_next_free_slab(); //should be some freelist structure (unimplemented atm)

  public:
    TaskManager();
    TaskDescriptor* next_scheduled_task();
};
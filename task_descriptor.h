#include <cstdint>

enum TaskState { ACTIVE, READY, EXITED, SEND_BLOCKED, RECEIVE_BLOCKED, REPLY_BLOCKED, EVENT_BLOCKED };

enum TaskPriority { LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4 };

class TaskDescriptor {
    uint64_t tid;
    TaskDescriptor* pid; //the kernel description suggests this should be a pointer, but I'd be fine with leaving it as just the ID
    TaskPriority priority;
    TaskDescriptor* ready_tid; //potentially unused
    TaskDescriptor* send_tid;  //potentially unused
    TaskState state;
    uint16_t* sp;

    static uint8_t next_tid;

   public:
    TaskDescriptor();

    uint64_t getTid();
    uint64_t getPid();
};
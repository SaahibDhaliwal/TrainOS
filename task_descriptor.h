#include <cstdint>

enum TaskSate { ACTIVE, READY, EXITED, SEND_BLOCKED, RECEIVE_BLOCKED, REPLY_BLOCKED, EVENT_BLOCKED };

enum TaskPriority { LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4 };

class TaskDescriptor {
    uint64_t tid;
    uint64_t pid;
    TaskPriority priority;
    TaskSate state;
    uint16_t* sp;

    static uint8_t next_tid;

   public:
    TaskDescriptor();
};
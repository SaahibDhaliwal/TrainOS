#ifndef __CONFIG__
#define __CONFIG__
#include <stdint.h>

namespace Config {
constexpr int MAX_PRIORITY = 64;
constexpr int MAX_TASKS = 64;
constexpr int USER_TASK_STACK_SIZE = 1024 * 1024 * 8;  // 8 MB
constexpr int USER_STACK_BASE = 0x3FFFFFF0;
constexpr int NAME_SERVER_TID = 2;
constexpr int NAME_SERVER_CAPACITY = 64;
constexpr int MAX_MESSAGE_LENGTH = 2048;
constexpr int MAX_COMMAND_LENGTH = 256;
constexpr int EXPERIMENT_COUNT = 2;
constexpr uint32_t TICK_SIZE = 10000;
constexpr int MARKLIN_QUEUE_SIZE = 8192;
constexpr int CONSOLE_QUEUE_SIZE = 32768;
constexpr int MAX_TRAINS = 6;
constexpr int NOTIFIER_PRIORITY = 15;

}  // namespace Config

#endif /* config.h */

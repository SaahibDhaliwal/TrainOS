#ifndef __CONFIG__
#define __CONFIG__

namespace Config {
constexpr int MAX_PRIORITY = 64;
constexpr int MAX_TASKS = 64;
constexpr int USER_TASK_STACK_SIZE = 1024 * 1024 * 4;  // 4 MB
constexpr int USER_STACK_BASE = 0x3FFFFFF0;
constexpr int NAME_SERVER_TID = 2;
constexpr int NAME_SERVER_CAPACITY = 64;
constexpr int MAX_MESSAGE_LENGTH = 256;
}  // namespace Config

#endif /* config.h */

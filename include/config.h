#ifndef __CONFIG__
#define __CONFIG__

namespace Config {
constexpr int MAX_PRIORITY = 64;
constexpr int MAX_TASKS = 64;
constexpr int USER_TASK_STACK_SIZE = 1024 * 4;
constexpr int USER_STACK_BASE = 0x3FFFFFF0;
}  // namespace Config

#endif /* config.h */

#ifndef __SYS_CALL_HANDLER__
#define __SYS_CALL_HANDLER__

#include <stdint.h>

enum class SYSCALL_NUM {
    CREATE,
    MY_TID,
    MY_PARENT_TID,
    YIELD,
    EXIT,
    SEND,
    RECEIVE,
    REPLY,
    AWAIT_EVENT,
    GET_IDLE,
    INTERRUPT = 50,
};

#endif /* sys_call_handler.h */
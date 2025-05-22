#include "rpi.h"
#include "task_descriptor.h"
#include "task.h"


void FirstUserTask(){
    // Assuming that the user task itself is at priority level 3

    //create two tasks at a lower priority
    int child_id = Create(LEVEL_2, &TestTask);
    uart_printf(CONSOLE, "Created: %u", child_id);
    child_id = Create(LEVEL_2, &TestTask);
    uart_printf(CONSOLE, "Created: %u", child_id);

    //create two tasks at a higher priority
    child_id = Create(LEVEL_4, &TestTask);
    uart_printf(CONSOLE, "Created: %u", child_id);
    child_id = Create(LEVEL_4, &TestTask);
    uart_printf(CONSOLE, "Created: %u", child_id);

    uart_printf(CONSOLE, "FirstUserTask: exiting");
    Exit();
}

void TestTask(){
    uart_printf(CONSOLE, "My tid: %u, my parent's tid: %u", MyTid(), MyParentTid());
    Yield();
    uart_printf(CONSOLE, "My tid: %u, my parent's tid: %u", MyTid(), MyParentTid());
    Exit();
}
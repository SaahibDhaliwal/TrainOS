#include "task_manager.h"
#include <stdint.h>


extern void vbar_init();
TaskManager::TaskManager() {
    vbar_init();
    // we could probably create/initialize the first user task here as well
}


void next_scheduled_task(){
    //look into our priority lists, determine the next one
    uint16_t* sp;
}


enum ErrCode{CREATE, MY_TID, MY_PARENT_TID, YIELD, EXIT}; 
extern uint32_t get_esr(); // in contextswitch.S

void TaskManager::handle_request(int error_code){
    
    //see D17-5658
    uint32_t error_code = get_esr() & 0xFFFFFF;
    
    if(error_code == CREATE){
        // get value in register x0 and x1 (with some helper function, might need some inline asm)
        // go through free list to determine next free stack we can use for this task
        // initialize TD with stack pointer address
        // get priority from x0
        // get entrypoint from x1 so we can point our eret to that on first execution (or just branch to it)


    } else if (error_code == MY_TID){
        //set x0 equal to:
        TaskManager::current_task->GetTid();

    } else if (error_code == MY_PARENT_TID){
        //same thing with:
        TaskManager::current_task->GetPid();
        
    } else if (error_code == YIELD){
        //place it at the end of the priority queue
    } else if (error_code == EXIT){
        //just... don't place it in a priority queue? So it'll never run?
        // I'm assuming that once a task is running, it's not in any priority queue.
        // Only once we svc, it gets put into a priority queue for it to wait until it is next ran.
        // That is, until we have tasks that are blocked. So we would track that somewhere else.
    } else {
        //panic
    }

    //remember that some of these have return values that should be placed into x0
    //  before calling kern2user. Also ensure the correct stack pointer for the task we want next is in x2

}
#include "task_manager.h"
#include "svc_handlers.cpp"
#include <stdint.h>


extern void vbar_init();
TaskManager::TaskManager() {
    vbar_init();
    // we could probably create/initialize the first user task here as well
    //
}


TaskDescriptor* TaskManager::next_scheduled_task(){
    //look into our priority lists, determine the next one
    uint16_t* sp;

    //set the current task into 

    //set the stack pointer of the task into x2, 
}




enum ErrCode{CREATE, MY_TID, MY_PARENT_TID, YIELD, EXIT}; 
extern uint32_t get_esr(); // in contextswitch.S


void TaskManager::handle_request(int error_code){
    
    //see D17-5658    
    uint32_t error_code = get_esr() & 0xFFFFFF;
    
    if(error_code == CREATE){
        // get value in register x1 and x2 (with some helper function, might need some inline asm)
        // go through free list to determine next free stack we can use for this task
        // initialize TD with stack pointer address
        // get priority from x1
        // get entrypoint from x2 so we can point our eret to that on first execution (or just branch to it)
        int priority = getRegister(1);
        int functionaddr = getRegister(2);
        char* location = get_next_free_slab();
        //dereference the memory address and put the TD there(?)
        //* location = TaskDescriptor(generate_tid(), TaskManager::current_task->getTid(), priority, new_stackpointer);



        //these would be on the user stack
        //setRegister(1, new_stackpointer)
        //setRegister(0, returnaddress)
        
        // SET UP THE STACK SO THAT WHATEVER HAPPENS BELLOW (FROM KERNEL2USER) WORKS
        // ldr x2, [sp, -16]
        // msr elr_el1, x0 // write the register to elr_el1
        
        // ldr x2, [sp, -24]
        // msr spsr_el1, x2
    
        // ldr x2, [sp, -32]
        // msr sp_el0, x2

         // If you put structure at a memory address, would it grow upwards? So if at 0x500, add structure, it would go to 0x550? And then stack would grow downward?



    } else if (error_code == MY_TID){
        //set x0 equal to:
        current_task->getTid();

    } else if (error_code == MY_PARENT_TID){
        //same thing with:
        current_task->getPid();
        
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

    //remember that some of these have return values that should be placed into THE USERS x0
    //  before calling kern2user. Also ensure the correct stack pointer for the task we want next is in x2

}


.macro callee_save_registers

    //each register is 8 bytes = 64 bits (duh)

    // 19 to 28 are the global registers 
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!
	stp x27, x28, [sp, #-16]!

    // 29 is frame pointer, 30 is the link register (return address)
	stp x29, x30, [sp, #-16]!

.endm


.macro restore_callee
	ldp x29, x30, [sp, #16]!
	ldp x27, x28, [sp, #16]!
	ldp x25, x26, [sp, #16]!
	ldp x23, x24, [sp, #16]!
	ldp x21, x22, [sp, #16]!
	ldp x19, x20, [sp, #16]!
	
.endm

.macro caller_save_registers
    // Save arguments
	stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!

    // there aren't enough registers to save a struct that is larger than
    //  7 bytes (because we only have 7 registers) so it must be passed as a pointer

    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!

    //temp for linker
    stp x16, x17, [sp, #-16]!

    //platform thread-local storage
    ldr x18, [sp, #-16]!
	
.endm

// Ensure that the stack pointer is pointed at where they were saved
.macro restore_caller
    ldr x18, [sp, #16]!
    ldp x16, x17, [sp, #16]!
    ldp x14, x15, [sp, #16]!
    ldp x12, x13, [sp, #16]!
    ldp x10, x11, [sp, #16]!
    ldp x8, x9, [sp, #16]!
    ldp x6, x7, [sp, #16]!
    ldp x4, x5, [sp, #16]!
    ldp x2, x3, [sp, #16]!
    ldp x0, x1, [sp, #16]!

.endm

.macro save_kernel
    caller_save_registers
    callee_save_registers
.endm

.macro restore_kernel
    restore_callee
    restore_caller
.endm


.global	user_to_kernel
user_to_kernel:
// https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception?lang=en#md244-taking-an-exception__stack-pointer-selection-and-stack-pointer-registers
    // As we enter the first-level handler at the vector table, we are using SP_ELx. This should by default be used to save the register context.
    // However, we typically then switch to use SP_EL0 instead for any further processing.
    
    //shifts arguments by one so we can use x0
    // if there are more than two arguments, add below
    mov x2, x1
    mov x1, x0
    // if you care about whats in x2, save it on the stack on your end...
    // moves stack pointer to the user one so that we save their stuff there
    mrs x0, sp_el0
    mov sp, x0
    //might be able to condense it into "mrs _el0, sp"

    //save their info on their stack
    caller_save_registers //potentially unneeded for A1
    callee_save_registers 


    mrs x0, elr_el1
    // write the register to where the stack pointer is pointing
    str x0, [sp, -16]
    
    mrs x0, spsr_el1
    str x0, [sp, -24]

    mrs x0, sp_el0
    str x0, [sp, -32]

    //change stack pointer back to el1, from before we did anything 

    mrs x0, sp_el1
    mov sp, x0
    

    restore_kernel

    // Ensure that the C handler code knows the exception code, provided in x0
    mrs x0, elr_el1

    // branch to C++ to handle whatever SVC code is provided
    bl x30


.global	kernel_to_user
kernel_to_user:
    
    save_kernel
   // pray that x2 has the stack pointer of the user task
    mov sp, x2
    
    ldr x2, [sp, -16]
    msr elr_el1, x0 // write the register to elr_el1
    
    ldr x2, [sp, -24]
    msr spsr_el1, x2

    ldr x2, [sp, -32]
    msr sp_el0, x2

    restore_callee
    restore_caller

    eret


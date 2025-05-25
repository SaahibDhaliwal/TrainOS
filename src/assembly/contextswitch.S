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


.macro restore_callee_v2
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
    // bl handle_request


.global	kernel2User
kernel2User:
    
    save_kernel
   // pray that x2 has the stack pointer of the user task
    mov sp, x2
    
    ldr x2, [sp, -16]
    msr elr_el1, x2 // write the register to elr_el1
    
    ldr x2, [sp, -24]
    msr spsr_el1, x2

    ldr x2, [sp, -32]
    msr sp_el0, x2

    restore_callee
    restore_caller

    eret

.global kernelToUser
kernelToUser:
    // x0 = kernelContext
    // x1 = userContext

    // Save kernelContext and userContext on kernel's stack so we can restor from/ store into when we do a userToKernel
    // so userContext is at sp
    // kernelContext is at sp + 8
    // str x0, [sp, #-8]!      // push x1, sp -= 8 again 
    stp x1, x0, [sp, #-16]!  

    // store callee-saved registers to kernelContext
    str x19, [x0, #(8 * 19)]
    str x20, [x0, #(8 * 20)]
    str x21, [x0, #(8 * 21)]
    str x22, [x0, #(8 * 22)]
    str x23, [x0, #(8 * 23)]
    str x24, [x0, #(8 * 24)]
    str x25, [x0, #(8 * 25)]
    str x26, [x0, #(8 * 26)]
    str x27, [x0, #(8 * 27)]
    str x28, [x0, #(8 * 28)]
    str x29, [x0, #(8 * 29)]
    str x30, [x0, #(8 * 30)]

   // mov x2, sp
    // str x2, [x0, #(8 * 31)]

    // str x0, [sp, #-16]!      // push x0, sp -= 8

    // restore user context

    // restore callee-saved registers from userContext
    ldr x19, [x1, #(8 * 19)]
    ldr x20, [x1, #(8 * 20)]
    ldr x21, [x1, #(8 * 21)]
    ldr x22, [x1, #(8 * 22)]
    ldr x23, [x1, #(8 * 23)]
    ldr x24, [x1, #(8 * 24)]
    ldr x25, [x1, #(8 * 25)]
    ldr x26, [x1, #(8 * 26)]
    ldr x27, [x1, #(8 * 27)]
    ldr x28, [x1, #(8 * 28)]
    ldr x29, [x1, #(8 * 29)]
    ldr x30, [x1, #(8 * 30)]

    // SP_EL0 
    ldr x2, [x1, #(8 * 31)]
    msr sp_el0, x2

    // ELR_EL1
    ldr x2, [x1, #(8 * 32)]
    msr elr_el1, x2

    // SPSR_EL1
    ldr x2, [x1, #(8 * 33)]
    msr spsr_el1, x2

    // return value from syscall
    ldr x0, [x1, #(8 * 0)]

    eret

.global userToKernel
userToKernel:
    // x3 = userContext
    // x4 = kernelContext

    // Pop userContext into x0
    // ldr x1, [sp], #16

    ldp x3, x4, [sp], #16

    // store callee-saved registers for userContext
    str x0, [x3, #(8 * 0)]
    str x1, [x3, #(8 * 1)]
    str x2, [x3, #(8 * 2)]
    str x19, [x3, #(8 * 19)]
    str x20, [x3, #(8 * 20)]
    str x21, [x3, #(8 * 21)]
    str x22, [x3, #(8 * 22)]
    str x23, [x3, #(8 * 23)]
    str x24, [x3, #(8 * 24)]
    str x25, [x3, #(8 * 25)]
    str x26, [x3, #(8 * 26)]
    str x27, [x3, #(8 * 27)]
    str x28, [x3, #(8 * 28)]
    str x29, [x3, #(8 * 29)]
    str x30, [x3, #(8 * 30)]

    // Save SP_EL0
    mrs x2, sp_el0
    str x2, [x3, #(8 * 31)]

    // Save ELR_EL1
    mrs x2, elr_el1
    str x2, [x3, #(8 * 32)]

    // Save SPSR_EL1
    mrs x2, spsr_el1
    str x2, [x3, #(8 * 33)]

    // restore callee-saved registers from kernelContext
    // ldr x2, [x4, #(8 * 31)]
   // mov sp, x2
    ldr x19, [x4, #(8 * 19)]
    ldr x20, [x4, #(8 * 20)]
    ldr x21, [x4, #(8 * 21)]
    ldr x22, [x4, #(8 * 22)]
    ldr x23, [x4, #(8 * 23)]
    ldr x24, [x4, #(8 * 24)]
    ldr x25, [x4, #(8 * 25)]
    ldr x26, [x4, #(8 * 26)]
    ldr x27, [x4, #(8 * 27)]
    ldr x28, [x4, #(8 * 28)]
    ldr x29, [x4, #(8 * 29)]
    ldr x30, [x4, #(8 * 30)]

    mrs x0, esr_el1

    ret
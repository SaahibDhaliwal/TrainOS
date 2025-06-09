.text
.align 2

.global Create
Create:
    svc 0
    ret

.global MyTid
MyTid:
    svc 1
    ret

.global MyParentTid
MyParentTid:
    svc 2
    ret

.global Yield
Yield:
    svc 3
    ret

.global Exit
Exit:
    svc 4
    ret

.global Send
Send:
    svc 5
    ret

.global Receive
Receive:
    svc 6
    ret

.global Reply
Reply:
    svc 7
    ret

.macro store_callee_registers context
    str x19, [\context, #(8 * 19)]
    str x20, [\context, #(8 * 20)]
    str x21, [\context, #(8 * 21)]
    str x22, [\context, #(8 * 22)]
    str x23, [\context, #(8 * 23)]
    str x24, [\context, #(8 * 24)]
    str x25, [\context, #(8 * 25)]
    str x26, [\context, #(8 * 26)]
    str x27, [\context, #(8 * 27)]
    str x28, [\context, #(8 * 28)]
    str x29, [\context, #(8 * 29)]
    str x30, [\context, #(8 * 30)]
.endm

.macro restore_callee_registers context
    ldr x19, [\context, #(8 * 19)]
    ldr x20, [\context, #(8 * 20)]
    ldr x21, [\context, #(8 * 21)]
    ldr x22, [\context, #(8 * 22)]
    ldr x23, [\context, #(8 * 23)]
    ldr x24, [\context, #(8 * 24)]
    ldr x25, [\context, #(8 * 25)]
    ldr x26, [\context, #(8 * 26)]
    ldr x27, [\context, #(8 * 27)]
    ldr x28, [\context, #(8 * 28)]
    ldr x29, [\context, #(8 * 29)]
    ldr x30, [\context, #(8 * 30)]
.endm

.macro store_caller_registers context
    str x0, [\context, #(8 * 0)]
    str x1, [\context, #(8 * 1)]
    str x2, [\context, #(8 * 2)]
    str x3, [\context, #(8 * 3)]
    str x4, [\context, #(8 * 4)]
    str x5, [\context, #(8 * 5)]
    //str x6, [\context, #(8 * 6)]
    //str x7, [\context, #(8 * 7)]
    str x8, [\context, #(8 * 8)]
    str x9, [\context, #(8 * 9)]
    str x10, [\context, #(8 * 10)]
    str x11, [\context, #(8 * 11)]
    str x12, [\context, #(8 * 12)]
    str x13, [\context, #(8 * 13)]
    str x14, [\context, #(8 * 14)]
    str x15, [\context, #(8 * 15)]
    str x16, [\context, #(8 * 16)]
    str x17, [\context, #(8 * 17)]
    str x18, [\context, #(8 * 18)]
.endm

.macro restore_caller_registers context
    // Note: we skip x0
    ldr x1, [\context, #(8 * 1)]
    ldr x2, [\context, #(8 * 2)]
    ldr x3, [\context, #(8 * 3)]
    ldr x4, [\context, #(8 * 4)]
    ldr x5, [\context, #(8 * 5)]
    ldr x6, [\context, #(8 * 6)]
    ldr x7, [\context, #(8 * 7)]
    ldr x8, [\context, #(8 * 8)]
    ldr x9, [\context, #(8 * 9)]
    ldr x10, [\context, #(8 * 10)]
    ldr x11, [\context, #(8 * 11)]
    ldr x12, [\context, #(8 * 12)]
    ldr x13, [\context, #(8 * 13)]
    ldr x14, [\context, #(8 * 14)]
    ldr x15, [\context, #(8 * 15)]
    ldr x16, [\context, #(8 * 16)]
    ldr x17, [\context, #(8 * 17)]
    ldr x18, [\context, #(8 * 18)]
.endm



.global kernelToUser
kernelToUser:
    // x0 = kernelContext
    // x1 = userContext

    // Save kernelContext and userContext on kernel's stack so we can restor from/ store into when we do a userToKernel
    // so userContext is at sp
    // kernelContext is at sp + 8
    stp x1, x0, [sp, #-16]!  

    // Store callee-saved registers to kernelContext
    store_callee_registers x0

    //mov x0, x1
    // Restore callee-saved registers from userContext
    restore_callee_registers x1

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
    // also restore their arguments?
    //ldr x2, [x1, #(8 * 2)]
    //ldr x3, [x1, #(8 * 3)]
    //ldr x4, [x1, #(8 * 4)]
    //ldr x1, [x1, #(8 * 1)]
    msr DAIFSet, #0b1001
    eret


.global userToKernel
userToKernel:
    // Restore kernelContext and userContext from kernel's stack (from when we did kernelToUser)
    // x6 = userContext
    // x7 = kernelContext
    ldp x6, x7, [sp], #16

    // Store callee-saved registers for userContext in the TD's context array
    store_callee_registers x6

    // Also store any arguments the user passed 
    str x0, [x6, #(8 * 0)]
    str x1, [x6, #(8 * 1)]
    str x2, [x6, #(8 * 2)]
    str x3, [x6, #(8 * 3)]
    str x4, [x6, #(8 * 4)]  // Message Send has 5 params

    // Save SP_EL0
    mrs x2, sp_el0
    str x2, [x6, #(8 * 31)]

    // Save ELR_EL1
    mrs x2, elr_el1
    str x2, [x6, #(8 * 32)]

    // Save SPSR_EL1
    mrs x2, spsr_el1
    str x2, [x6, #(8 * 33)]

    // Restore callee-saved registers from kernelContext
    restore_callee_registers x7

    mrs x0, esr_el1

    ret

.global slowUserToKernel
slowUserToKernel:
    stp x6, x7, [sp, #-16]! //we store x7, even though we don't need to. Just to keep sp stack aligned
    // Restore kernelContext and userContext from kernel's stack (from when we did kernelToUser)
    // x0 = userContext
    // x1 = kernelContext
    //add sp, sp, #16
    ldp x6, x7, [sp, #16]
    //ldp x6, x7, [sp], #16
    //sub sp, sp, #16
    //add sp , sp, #32
    //ldr x0, [sp, #16] //get user context

    // Store callee-saved registers for userContext in the TD's context array
    store_callee_registers x6
        // Restore callee-saved registers from kernelContext
    restore_callee_registers x7

    store_caller_registers x6

    // Save SP_EL0
    mrs x2, sp_el0
    str x2, [x6, #(8 * 31)]

    // Save ELR_EL1
    mrs x2, elr_el1
    str x2, [x6, #(8 * 32)]

    // Save SPSR_EL1
    mrs x2, spsr_el1
    str x2, [x6, #(8 * 33)]

    ldr x7, [sp, #(8 * 1)] 
    str x7, [x6, #(8 * 7)]
    //mov x7, x0
    ldr x7, [sp, #(8 * 0)]
    str x7, [x6, #(8 * 7)]

    //ldp x6, x7, [sp], #16
    add sp, sp, #32
    //store locals
    //store_caller_registers x0
    


    mov x0, #8

    ret


.global slowKernelToUser
slowKernelToUser:
    // x0 = kernelContext
    // x1 = userContext

    // Save kernelContext and userContext on kernel's stack so we can restor from/ store into when we do a userToKernel
    // so userContext is at sp
    // kernelContext is at sp + 8
    stp x1, x0, [sp, #-16]!  

    // Store callee-saved registers to kernelContext
    store_callee_registers x0
    mov x0, x1

    // Restore callee-saved registers from userContext
    restore_callee_registers x0

    // SP_EL0 
    ldr x2, [x0, #(8 * 31)]
    msr sp_el0, x2

    // ELR_EL1
    ldr x2, [x0, #(8 * 32)]
    msr elr_el1, x2

    // SPSR_EL1
    ldr x2, [x0, #(8 * 33)]
    
    msr spsr_el1, x2

    restore_caller_registers x0

    // return value from syscall
    ldr x0, [x0, #(8 * 0)]

    msr DAIFSet, #0b1001
    eret

.global fillCallerSavedRegisters
fillCallerSavedRegisters:
    // x0 has the base
    add x1, x0, #1
    add x2, x0, #2
    add x3, x0, #3
    add x4, x0, #4
    add x5, x0, #5
    add x6, x0, #6
    add x7, x0, #7
    add x8, x0, #8
    add x9, x0, #9
    add x10, x0, #10
    add x11, x0, #11
    add x12, x0, #12
    add x13, x0, #13
    add x14, x0, #14
    add x15, x0, #15
    add x16, x0, #16
    add x17, x0, #17
    add x18, x0, #18
    ret

.global dumpRegisters
dumpRegisters:
    // x0 has the buff
    str x0, [x0]
    add x0, x0, #8
    stp x1, x2, [x0], #16
    stp x3, x4, [x0], #16
    stp x5, x6, [x0], #16
    stp x7, x8, [x0], #16
    stp x9, x10, [x0], #16
    stp x11, x12, [x0], #16
    stp x13, x14, [x0], #16
    stp x15, x16, [x0], #16
    stp x17, x18, [x0], #16
    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, x30, [x0], #16
    ret


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

    eret

.global userToKernel
userToKernel:
    // Restore kernelContext and userContext from kernel's stack (from when we did kernelToUser)
    // x3 = userContext
    // x4 = kernelContext
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
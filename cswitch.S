.text
.align 2
.global	to_user

.macro save_regs
	// Save registers

    // Subtracts 17*16 (272 bytes) from the current stack pointer (sp).
    //  so this allocates 272 bytes on the stack.
	sub sp, sp, 17*16

    // Some local arguments
	stp x0, x1, [sp, 0*16]

    // 19 to 28 are the global registers 
	stp x19, x20, [sp, 9*16]
	stp x21, x22, [sp, 10*16]
	stp x23, x24, [sp, 11*16]
	stp x25, x26, [sp, 12*16]
	stp x27, x28, [sp, 13*16]

    // 29 is frame pointer, 30 is the link register (return address)
	stp x29, x30, [sp, 14*16]

	
.endm

to_user:
// if you've been called, save these registers
  save_regs

  //change to system state;


  // get the sp, spsr and return value of the active task from its TD

  //restore ELR_EL1, SPSR_EL1, SP_EL0
  ret

// push registers onto stack
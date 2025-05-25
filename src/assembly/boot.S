// ***************************************
// SCTLR_EL1, System Control Register (EL1)
// Architecture Reference Manual Section D17.2.118
// ***************************************

// trapping means not executing natively, i.e. for fp instructions not using hardware
// but instead jumping to a exception handler defined in the exception vector table

// reserved bits are just not used by the CPU, some are 0 some are 1
#define SCTLR_RESERVED (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
// WFE pauses CPU execution until event occurs, CPU enters standby
// so we don't go into standby for interrupts or events
#define SCTLR_WFE_WFI  (1 << 18) | (1 << 16)


// ***************************************
// CPACR_EL1, Architectural Feature Access Control Register (EL1)
// Architecture Reference Manual Section D17.2.30
// CPTR_EL2, Architectural Feature Trap Register (EL2)
// Architecture Reference Manual Section D17.2.31
// ***************************************
// FPEN is for floating point, we won't trap advanced instructions that use fp registers
#define FPEN_TRAP (3 << 20)
// zen is supercharge math hardware
#define  ZEN_TRAP (3 << 16)

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2)
// Architecture Reference Manual Section D17.2.48
// ***************************************
#define HCR_RW (1 << 31)
// EL1 is 64 bit

// ***************************************
// SPSR_EL2, Saved Program Status Register (EL2)
// Architecture Reference Manual Section C5.2.19
// ***************************************
// D saves pstate.D, debug
// I saves pstate.I, interrupt requests
// F saves pstate.F, fast interrupt requests
#define SPSR_MASK_ALL (11 << 6) // mask D,I,F
// reserved
#define SPSR_EL1       (5 << 0) // EL1 with SP_EL1

// ensure the linker puts this at the start of the image
// assembler groups all code with this tag into a section name .text.boot in the .o file
// just needs so linker can organize -> linker just organizes object file
.section ".text.boot" 
.global _start // makes _start visible to everyone, so now the linker can start from _start
_start:
	// if this is not Core 0, exit to wfe-loop
	mrs  x0, mpidr_el1 // moves system register mpidr_el1 into x0, mpidr_el1 is our core identificiation mechanism
	and  x0, x0, #3	   // bit mask to see which cores we have
	cbnz x0, exit	   // if core number is not 0, branch to exit

	// enable SMP cores (needed for MMU, caching)
	mrs x0, s3_1_c15_c2_1
	orr x0, x0, #(0x1 << 6)
	msr s3_1_c15_c2_1, x0

	// are we already in EL1?
	mrs	x1, CurrentEL
	and	x1, x1, #8
	cbz	x1, el1_entry

	// disable EL1 trap on FP/SIMD/SVE instructions
	ldr x2, =(FPEN_TRAP | ZEN_TRAP)
	msr cptr_el2, x2 // write x2 into cptr_el2

	// set execution state for EL1 to AArch64
	ldr x3, =HCR_RW
	msr hcr_el2, x3

	// switch to EL1 by fake exception return
	ldr x4, =(SPSR_MASK_ALL | SPSR_EL1) 
	msr spsr_el2, x4   // turn on the saving pstate for el2
	adr x5, el1_entry 
	msr elr_el2, x5	   // so we tell el2 to resume execution at el1 entry

	eret // -> el1_entry // exception return

el1_entry:
	// configure processor
	ldr x0, =(SCTLR_RESERVED | SCTLR_WFE_WFI)
	msr sctlr_el1, x0

	// disable EL0 trap on FP/SIMD/SVE instructions
	ldr x1, =(FPEN_TRAP | ZEN_TRAP)
	msr cpacr_el1, x1

	// initialize SP (initial stack below text here)
	ldr x2, =_start 
	mov sp, x2 // stack pointer starts _start
	// branch and link to main routine in C/C++
	bl kmain

.global _reboot
_reboot:
	// use watchdog to reboot
	mov x2, 36
	mov x0, 28
	movk	x2, 0xfe10, lsl 16
	movk	x0, 0xfe10, lsl 16
	mov w3, 1
	mov w1, 32
	movk	w3, 0x5a00, lsl 16
	str w3, [x2]
	movk	w1, 0x5a00, lsl 16
	str w1, [x0]
	// shouldn't be reached

exit:
	wfe			// wait for event, cpu in standby
	b exit

	.arch armv8-a
	.file	"asm.c"
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"spsr: 0x%x\n\r"
	.text
	.align	2
	.p2align 5,,15
	.global	dummy
	.type	dummy, %function
dummy:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	str	x19, [sp, 16]
	// Start of user assembly
// 7 "asm.c" 1
	mrs x19, spsr_el1
// 0 "" 2
	// End of user assembly
	adrp	x0, .LC0
	mov	w1, w19
	add	x0, x0, :lo12:.LC0
	bl	fakeprintf
	add	w0, w19, 1
	// Start of user assembly
// 11 "asm.c" 1
	msr spsr_el1, x0
// 0 "" 2
	// End of user assembly
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	dummy, .-dummy
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"

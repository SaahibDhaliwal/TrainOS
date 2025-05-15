	.arch armv8-a
	.file	"main.c"
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"%d\n"
	.section	.text.startup,"ax",@progbits
	.align	2
	.p2align 5,,15
	.global	main
	.type	main, %function
main:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	ldr	x0, [x1, 8]
	str	x19, [sp, 16]
	mov	x19, x1
	bl	atoi
	mov	w1, w0
	ldr	x0, [x19, 16]
	mov	w19, w1
	bl	atoi
	mov	w1, w0
	mov	w0, w19
	bl	adder //gets called here
	mov	w1, w0
	adrp	x0, .LC0
	add	x0, x0, :lo12:.LC0
	bl	printf
	ldr	x19, [sp, 16]
	mov	w0, 0
	ldp	x29, x30, [sp], 32
	ret
	.size	main, .-main
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"

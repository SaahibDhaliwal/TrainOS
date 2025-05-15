	.arch armv8-a
	.file	"main3.c"
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
	stp	x29, x30, [sp, -16]!
	adrp	x0, .LANCHOR0
	mov	w2, 48879
	mov	x29, sp
	movk	w2, 0xdead, lsl 16
	str	w2, [x0, #:lo12:.LANCHOR0]
	ldr	x0, [x1, 8]
	bl	atoi
	bl	data_adder
	mov	w1, w0
	adrp	x0, .LC0
	add	x0, x0, :lo12:.LC0
	bl	printf
	mov	w0, 0
	ldp	x29, x30, [sp], 16
	ret
	.size	main, .-main
	.global	some_data
	.bss
	.align	2
	.set	.LANCHOR0,. + 0
	.type	some_data, %object
	.size	some_data, 4
some_data:
	.zero	4
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"

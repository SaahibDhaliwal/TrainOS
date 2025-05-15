	.arch armv8-a
	.file	"adder10.c"
	.text
	.align	2
	.p2align 5,,15
	.global	adder
	.type	adder, %function
adder:
	add	w0, w0, w1
	ldr	w1, [sp, 8] // the memory location 8 bytes after where sp points t
	add	w0, w0, w2
	ldr	w2, [sp]
	add	w0, w0, w3
	add	w0, w0, w4
	add	w0, w0, w5
	add	w0, w0, w6
	add	w0, w0, w7
	add	w0, w0, w2
	add	w0, w0, w1
	ret
	.size	adder, .-adder
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"

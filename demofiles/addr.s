	.arch armv8-a
	.file	"adder.c"
	.text
	.align	2
	.p2align 5,,15
	.global	adder
	.type	adder, %function
adder:
	add	w0, w0, w1 // means registers 0, 1, lowest bits
	ret
	.size	adder, .-adder
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"

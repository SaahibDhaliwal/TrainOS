.text
.align 2
.global	adder
.type adder, %function
adder:
  add x2, x0, x1
  mov x0, x2
  ret

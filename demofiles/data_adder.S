.text
.align 2
.global	data_adder
data_adder:
  ldr x1, =some_data
  ldr x1, [x1]
  add x2, x1, x0
  mov x0, x2
  ret

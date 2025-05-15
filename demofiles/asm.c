extern void fakeprintf(const char *format, ...);

/* https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html */
unsigned int dummy() {
  unsigned int val;
  // read via asm - use labelled argument [zzz]
  asm volatile("mrs %[zzz], spsr_el1" : [zzz] "=r"(val));
  fakeprintf("spsr: 0x%x\n\r", val);
  val += 1;
  // write via asm - positional argument %0
  asm volatile("msr spsr_el1, %0" :: "r"(val));
  return val;
}

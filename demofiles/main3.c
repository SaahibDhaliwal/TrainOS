#include <stdio.h>
#include <stdlib.h>

extern unsigned int data_adder(unsigned int);

unsigned int some_data;

int main(int argc, char** argv) {
  some_data = 0xdeadbeef;
  unsigned int a1 = atoi(argv[1]); 
  // adds first parameter to some_data
  unsigned int result = data_adder(a1);
  printf("%d\n", result);
  return 0;
}

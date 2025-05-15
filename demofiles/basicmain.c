#include <stdio.h>
#include <stdlib.h>

extern int adder(int, int);

int main(int argc, char** argv) {
  int a1 = atoi(argv[1]);
  int a2 = atoi(argv[2]);
  int result = adder(a1, a2);
  printf("%d\n", result);
  return 0;
}

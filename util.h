#ifndef _util_h_
#define _util_h_ 1

int a2d(char ch);
char a2i(char ch, char **src, int base, int *nump);
void ui2a(unsigned int num, unsigned int base, char *bf);
void i2a(int num, char *bf);

#endif /* util.h */

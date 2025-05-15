#ifndef _rpi_h_
#define _rpi_h_ 1

#include <stddef.h>

#define CONSOLE 1
#define MARKLIN 2

void gpio_init();
void uart_config_and_enable(size_t line);
unsigned char uart_getc(size_t line);
void uart_putc(size_t line, char c);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_puts(size_t line, const char *buf);
void uart_printf(size_t line, const char *fmt, ...);

#endif /* rpi.h */

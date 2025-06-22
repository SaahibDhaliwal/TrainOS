#ifndef __RPI__
#define __RPI__

#include <stddef.h>
#include <stdint.h>

#define CONSOLE 1
#define MARKLIN 2

void gpio_init();
void uart_config_and_enable(size_t line);
unsigned char uart_getc(size_t line);
void uart_putc(size_t line, char c);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_puts(size_t line, const char *buf);
void uart_printf(size_t line, const char *fmt, ...);

enum class IMSC { CTS, RX, TX, RT, COUNT };
enum class MIS { CTS = 1, RX = 8, TX = 16, RT = 32 };  // 111001

void uartSetIMSC(size_t line, IMSC input);
void uartClearIMSC(size_t line, IMSC input);
int uartCheckMIS(size_t line);
void uartClearICR(size_t line, IMSC input);
bool uartRXEmpty(size_t line);
bool uartTXFull(size_t line);
void uartPutTX(size_t line, char c);
unsigned char uartGetRX(size_t line);
void uartPutConsoleC(uint32_t tid, char c);
void uartPutConsoleS(uint32_t tid, const char *buf);
void uartPrintf(uint32_t tid, const char *fmt, ...);

#endif /* rpi.h */

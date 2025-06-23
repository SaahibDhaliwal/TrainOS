#include "rpi.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "console_server.h"
#include "console_server_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "test_utils.h"
#include "util.h"

static char* const MMIO_BASE = (char*)0xFE000000;

/*********** GPIO CONFIGURATION ********************************/

static char* const GPIO_BASE = (char*)(MMIO_BASE + 0x200000);
static const uint32_t GPFSEL_OFFSETS[6] = {0x00, 0x04, 0x08, 0x0c, 0x10, 0x14};
static const uint32_t GPIO_PUP_PDN_CNTRL_OFFSETS[4] = {0xe4, 0xe8, 0xec, 0xf0};

#define GPFSEL_REG(reg) (*(uint32_t*)(GPIO_BASE + GPFSEL_OFFSETS[reg]))
#define GPIO_PUP_PDN_CNTRL_REG(reg) (*(uint32_t*)(GPIO_BASE + GPIO_PUP_PDN_CNTRL_OFFSETS[reg]))

// function control settings for GPIO pins
static const uint32_t GPIO_INPUT = 0x00;
static const uint32_t GPIO_OUTPUT = 0x01;
static const uint32_t GPIO_ALTFN0 = 0x04;
static const uint32_t GPIO_ALTFN1 = 0x05;
static const uint32_t GPIO_ALTFN2 = 0x06;
static const uint32_t GPIO_ALTFN3 = 0x07;
static const uint32_t GPIO_ALTFN4 = 0x03;
static const uint32_t GPIO_ALTFN5 = 0x02;

// pup/pdn resistor settings for GPIO pins
static const uint32_t GPIO_NONE = 0x00;
static const uint32_t GPIO_PUP = 0x01;
static const uint32_t GPIO_PDP = 0x02;

static void setup_gpio(uint32_t pin, uint32_t setting, uint32_t resistor) {
    uint32_t reg = pin / 10;
    uint32_t shift = (pin % 10) * 3;
    uint32_t status = GPFSEL_REG(reg);  // read status
    status &= ~(7u << shift);           // clear bits
    status |= (setting << shift);       // set bits
    GPFSEL_REG(reg) = status;

    reg = pin / 16;
    shift = (pin % 16) * 2;
    status = GPIO_PUP_PDN_CNTRL_REG(reg);  // read status
    status &= ~(3u << shift);              // clear bits
    status |= (resistor << shift);         // set bits
    GPIO_PUP_PDN_CNTRL_REG(reg) = status;  // write back
}

// GPIO initialization, to be called before UART functions.
// For UART3 (line 2 on the RPi hat), we need to configure the GPIO to route
// the uart control and data signals to the GPIO pins 4-7 expected by the hat.
// GPIO pins 14 & 15 already configured by boot loader, but redo for clarity.
void gpio_init() {
    setup_gpio(4, GPIO_ALTFN4, GPIO_NONE);
    setup_gpio(5, GPIO_ALTFN4, GPIO_NONE);
    setup_gpio(6, GPIO_ALTFN4, GPIO_NONE);
    setup_gpio(7, GPIO_ALTFN4, GPIO_NONE);
    setup_gpio(14, GPIO_ALTFN0, GPIO_NONE);
    setup_gpio(15, GPIO_ALTFN0, GPIO_NONE);
}

/*********** UART CONTROL ************************ ************/

static char* const UART0_BASE = (char*)(MMIO_BASE + 0x201000);
static char* const UART3_BASE = (char*)(MMIO_BASE + 0x201600);

static char* const line_uarts[] = {NULL, UART0_BASE, UART3_BASE};
#define UART_REG(line, offset) (*(volatile uint32_t*)(line_uarts[line] + offset))

// UART register offsets
static const uint32_t UART_DR = 0x00;
static const uint32_t UART_FR = 0x18;
static const uint32_t UART_IBRD = 0x24;
static const uint32_t UART_FBRD = 0x28;
static const uint32_t UART_LCRH = 0x2c;
static const uint32_t UART_CR = 0x30;
static const uint32_t UART_IMSC = 0x38;
static const uint32_t UART_MIS = 0x40;
static const uint32_t UART_ICR = 0x44;

// masks for specific fields in the UART registers
static const uint32_t UART_FR_BUSY = 0x08;
static const uint32_t UART_FR_RXFE = 0x10;
static const uint32_t UART_FR_TXFF = 0x20;
static const uint32_t UART_FR_RXFF = 0x40;
static const uint32_t UART_FR_TXFE = 0x80;

static const uint32_t UART_CR_UARTEN = 0x01;
static const uint32_t UART_CR_LBE = 0x80;
static const uint32_t UART_CR_TXE = 0x100;
static const uint32_t UART_CR_RXE = 0x200;
static const uint32_t UART_CR_RTS = 0x800;
static const uint32_t UART_CR_RTSEN = 0x4000;
static const uint32_t UART_CR_CTSEN = 0x8000;

static const uint32_t UART_LCRH_PEN = 0x2;
static const uint32_t UART_LCRH_EPS = 0x4;
static const uint32_t UART_LCRH_STP2 = 0x8;
static const uint32_t UART_LCRH_FEN = 0x10;
static const uint32_t UART_LCRH_WLEN_LOW = 0x20;
static const uint32_t UART_LCRH_WLEN_HIGH = 0x40;

static const uint32_t UART_IMSC_CTS = 0x02;  // 0x02 -> 0010 so AND would isolate that bit
static const uint32_t UART_IMSC_RX = 0x10;
static const uint32_t UART_IMSC_TX = 0x20;
static const uint32_t UART_IMSC_RT = 0x40;  // receive timeout

void uartSetIMSC(size_t line, IMSC input) {
    // UART_REG(line, UART_IMSC) = 0x7FF;
    switch (input) {
        case IMSC::CTS:
            UART_REG(line, UART_IMSC) |= UART_IMSC_CTS;
            break;
        case IMSC::RX:
            UART_REG(line, UART_IMSC) |= UART_IMSC_RX;
            break;
        case IMSC::TX:
            UART_REG(line, UART_IMSC) |= UART_IMSC_TX;
            break;
        case IMSC::RT:
            UART_REG(line, UART_IMSC) |= UART_IMSC_RT;
            break;
        default:
            ASSERT(0, "Clear ICR imsc case failed\n\r");
            // print some error
            break;
    }
}

// since
void uartClearIMSC(size_t line, IMSC input) {
    switch (input) {
        case IMSC::CTS:
            ASSERT(UART_REG(line, UART_IMSC) & UART_IMSC_CTS);  // it must be enabled for us to disable
            UART_REG(line, UART_IMSC) ^= UART_IMSC_CTS;
            break;
        case IMSC::RX:
            ASSERT(UART_REG(line, UART_IMSC) & UART_IMSC_RX);
            UART_REG(line, UART_IMSC) ^= UART_IMSC_RX;
            break;
        case IMSC::TX:
            ASSERT(UART_REG(line, UART_IMSC) & UART_IMSC_TX);
            UART_REG(line, UART_IMSC) ^= UART_IMSC_TX;
            break;
        case IMSC::RT:
            ASSERT(UART_REG(line, UART_IMSC) & UART_IMSC_RT);
            UART_REG(line, UART_IMSC) ^= UART_IMSC_RT;
            break;
        default:
            // print some error
            ASSERT(0, "Clear ICR imsc case failed\n\r");

            break;
    }
}

int uartCheckMIS(size_t line) {
    return (UART_REG(line, UART_MIS) & 0x72) >> 1;  // AND 1110010 >> to 111001
    // If odd, sub 1 and bit shift
    // If larger than 32, subtract 32 (RT) and continue
    // If not equal to 16, then subtract 16 and so on
    // Goes: RT, TX, RX, junk, junk, CTS
}

// Thankfully the offsets are the same so we can use the predefined IMSC
void uartClearICR(size_t line, IMSC input) {
    switch (input) {
        case IMSC::CTS:
            // ASSERT(UART_REG(line, UART_ICR) & UART_IMSC_CTS);  // it must be enabled for us to disable
            UART_REG(line, UART_ICR) = UART_IMSC_CTS;
            break;
        case IMSC::RX:
            // ASSERT(UART_REG(line, UART_ICR) & UART_IMSC_RX);
            UART_REG(line, UART_ICR) = UART_IMSC_RX;
            break;
        case IMSC::TX:
            // ASSERT(UART_REG(line, UART_ICR) & UART_IMSC_TX);
            UART_REG(line, UART_ICR) = UART_IMSC_TX;
            break;
        case IMSC::RT:
            // ASSERT(UART_REG(line, UART_ICR) & UART_IMSC_RT);
            UART_REG(line, UART_ICR) = UART_IMSC_RT;
            break;
        default:
            // print some error
            ASSERT(0, "Clear ICR imsc case failed\n\r");
            break;
    }
}

bool uartRXEmpty(size_t line) {
    return (UART_REG(line, UART_FR) & UART_FR_RXFE);
}

bool uartTXFull(size_t line) {
    return (UART_REG(line, UART_FR) & UART_FR_TXFF);
}

void uartPutTX(size_t line, char c) {
    UART_REG(line, UART_DR) = c;
}

unsigned char uartGetRX(size_t line) {
    return UART_REG(line, UART_DR);
}

// Configure the line properties (e.g, parity, baud rate) of a UART and ensure that it is enabled
void uart_config_and_enable(size_t line) {
    uint32_t baud_ival, baud_fval;
    uint32_t stop2;

    switch (line) {
        // setting baudrate to approx. 115246.09844 (best we can do); 1 stop bit
        case CONSOLE:
            baud_ival = 26;
            baud_fval = 2;
            stop2 = 0;
            break;
        // setting baudrate to 2400; 2 stop bits
        case MARKLIN:
            baud_ival = 1250;
            baud_fval = 0;
            stop2 = UART_LCRH_STP2;
            break;
        default:
            return;
    }

    // line control registers should not be changed while the UART is enabled, so disable it
    uint32_t cr_state = UART_REG(line, UART_CR);
    UART_REG(line, UART_CR) = cr_state & ~UART_CR_UARTEN;

    // set the baud rate
    UART_REG(line, UART_IBRD) = baud_ival;
    UART_REG(line, UART_FBRD) = baud_fval;

    if (line == CONSOLE) {
        // set the line control registers: 8 bit, no parity, 1 or 2 stop bits, FIFOs enabled
        UART_REG(line, UART_LCRH) = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW | UART_LCRH_FEN | stop2;
    } else {
        // set the line control registers: 8 bit, no parity, 1 or 2 stop bits, FIFOs DISABLED
        UART_REG(line, UART_LCRH) = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW | stop2;
    }

    // re-enable the UART; enable both transmit and receive regardless of previous state
    UART_REG(line, UART_CR) = cr_state | UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

unsigned char uart_getc(size_t line) {
    unsigned char ch;
    /* wait for data if necessary */
    while (UART_REG(line, UART_FR) & UART_FR_RXFE);
    ch = UART_REG(line, UART_DR);
    return ch;
}

void uart_putc(size_t line, char c) {
    // make sure there is room to write more data
    while (UART_REG(line, UART_FR) & UART_FR_TXFF);
    UART_REG(line, UART_DR) = c;
}

void uart_putl(size_t line, const char* buf, size_t blen) {
    for (size_t i = 0; i < blen; i++) {
        uart_putc(line, *(buf + i));
    }
}

void uart_puts(size_t line, const char* buf) {
    while (*buf) {
        uart_putc(line, *buf);
        buf++;
    }
}

void uart_printf(size_t line, const char* fmt, ...) {
    va_list va;
    char ch, buf[12];

    va_start(va, fmt);
    while ((ch = *(fmt++))) {
        if (ch != '%')
            uart_putc(line, ch);
        else {
            ch = *(fmt++);
            switch (ch) {
                case 'u':
                    ui2a(va_arg(va, unsigned int), 10, buf);
                    uart_puts(line, buf);
                    break;
                case 'd':
                    i2a(va_arg(va, int), buf);
                    uart_puts(line, buf);
                    break;
                case 'x':
                    ui2a(va_arg(va, unsigned int), 16, buf);
                    uart_puts(line, buf);
                    break;
                case 'c':
                    uart_putc(line, (char)va_arg(va, int));
                    break;
                case 's':
                    uart_puts(line, va_arg(va, char*));
                    break;
                case '%':
                    uart_putc(line, ch);
                    break;
                case '\0':
                    return;
            }
        }
    }
    va_end(va);
}

#include "gic.h"

#include <stdarg.h>

#include "rpi.h"

static char* const MMIO_BASE = (char*)0xFE000000;
static char* const GIC_BASE = (char*)(0xFF840000);
static char* const GICD_BASE = (char*)GIC_BASE + 0x1000;
static char* const GICC_BASE = (char*)GIC_BASE + 0x2000;

static const uint32_t GICC_CTLR = 0x00;
static const uint32_t GICD_CTLR = 0x00;

static const uint32_t GICD_ISENABLE = 0x100;
static const uint32_t GICD_ITARGETSR = 0x800;

static const uint32_t GICC_IAR = 0xC;
static const uint32_t GICC_EOIR = 0x10;

#define GICC_REG(offset) (*(volatile uint32_t*)(GICC_BASE + offset))
#define GICD_REG(offset) (*(volatile uint32_t*)(GICD_BASE + offset))

void gicTarget(uint32_t interrupt_id) {
    *(GICD_BASE + GICD_ITARGETSR + interrupt_id) |= 1;  // (figure 4-14))
}

void gicEnable(int interrupt_id) {
    volatile uint32_t* enable = reinterpret_cast<uint32_t*>(
        GICD_BASE + GICD_ISENABLE + (4 * (interrupt_id / 32)));  // gets the enabler number and required offset
    *enable = (0x1 << (interrupt_id % 32));                      // shift to set the requires Set-enable bit
}

void gicInit() {
    // GICC_REG(GICC_CTLR) = GICC_REG(GICC_CTLR) | 0x1;
    // GICD_REG(GICD_CTLR) = GICD_REG(GICD_CTLR) | 0x1;
    // all interrupts disabled at GIC? Above isn't needed?
    // uart_printf(CONSOLE, "gic has been init\n\r");
    gicTarget(97);   // route interrupt to IRQ on CPU 0
    gicEnable(97);   // enable interrupt
    gicTarget(153);  // uart
    gicEnable(153);
}

void gicEndInterrupt(int interrupt_id) {
    GICC_REG(GICC_EOIR) = interrupt_id;
}

uint32_t gicGetInterrupt(void) {
    return GICC_REG(GICC_IAR);
}

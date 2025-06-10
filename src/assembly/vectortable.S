.text
.align 2
.global	vbar_init
vbar_init:
    ldr x0, =vbar_baseaddr
    msr VBAR_EL1, x0
    //br x30
    ret


// See https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception?lang=en

#define SPSR_MASK_ALL (11 << 6) // mask D,I,F  (1011)
#define SPSR_CLEARBITS #0xFFFFFFFFFFFFFC3F  // Binary, all ones except for four zeros at position 6,7,8,9
#define SPSR_EL1       (5 << 0) // EL1 with SP_EL1

.balign 0x800 // this is 2048
vbar_baseaddr:
    same_el_inEL0_SYNC:
        b dummy_handler

    .balign 0x80 // this is 128
    same_el_inEL0_IRQ:
        b dummy_handler

    .balign 0x80
    same_el_inEL0_FIQ:
        b dummy_handler

    .balign 0x80
    same_el_inEL0_SERROR:
        b dummy_handler

    // ---------------------------

    .balign 0x80 
    same_el_inELX_SYNCH:
        b test_handler

    .balign 0x80 
    same_el_inELX_IRQ:
        b dummy_handler

    .balign 0x80 
    same_el_inELX_FIQ:
        b dummy_handler

    .balign 0x80 
    same_el_inELX_SERROR:
        b dummy_handler

// ---------------------------------
    .balign 0x80 
    lower_el_AARCH64_SYNCH:
        b slowUserToKernel          //Important one for A1
    
    .balign 0x80 
    lower_el_AARCH64_IRQ:
        // see https://tc.gts3.org/cs3210/2020/spring/l/lec14/lec14.html#cover
        msr DAIFSet, #0b1011
        b slowUserToKernel          // asynch interrupt

    .balign 0x80 
    lower_el_AARCH64_FIQ:
        b dummy_handler         // asynch interrupt, but unused

    .balign 0x80 
    lower_el_AARCH64_SERROR:
        b dummy_handler

// -----------------------------------
    .balign 0x80 
    lower_el_AARCH32_SYNCH:
        b dummy_handler

    .balign 0x80 
    lower_el_AARCH32_IRQ:
        b dummy_handler

    .balign 0x80 
    lower_el_AARCH32_FIQ:
        b dummy_handler

    .balign 0x80 
    lower_el_AARCH32_SERROR:
        b dummy_handler
      

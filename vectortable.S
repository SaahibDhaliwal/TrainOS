.text
.align 2
.global	vbar_init
vbar_init:
    ldr x0, =vbar_baseaddr
    msr VBAR_EL1, x0
    br x30

.align 2
default_handler:
    wfe
	b default_handler


// See https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception?lang=en

.align 11 // this is 2048
vbar_baseaddr:
    same_el_inEL0_SYNC:
        bl default_handler

    .align 7 // this is 128
    same_el_inEL0_IRQ:
        bl default_handler

    .align 7 
    same_el_inEL0_FIQ:
        bl default_handler

    .align 7 
    same_el_inEL0_SERROR:
        bl default_handler

    // ---------------------------

    .align 7 
    same_el_inELX_SYNCH:
        bl default_handler

    .align 7 
    same_el_inELX_IRQ:
        bl default_handler

    .align 7 
    same_el_inELX_FIQ:
        bl default_handler

    .align 7 
    same_el_inELX_SERROR:
        bl default_handler

// ---------------------------------
    .align 7 
    lower_el_AARCH64_SYNCH:
        bl kernel2user          //Important one for A1

    .align 7 
    lower_el_AARCH64_IRQ:
        bl default_handler

    .align 7 
    lower_el_AARCH64_FIQ:
        bl default_handler

    .align 7 
    lower_el_AARCH64_SERROR:
        bl default_handler

// -----------------------------------
    .align 7 
    lower_el_AARCH32_SYNCH:
        bl default_handler

    .align 7 
    lower_el_AARCH32_IRQ:
        bl default_handler

    .align 7 
    lower_el_AARCH32_FIQ:
        bl default_handler

    .align 7 
    lower_el_AARCH32_SERROR:
        bl default_handler
      

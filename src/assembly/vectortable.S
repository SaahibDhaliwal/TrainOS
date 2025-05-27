.text
.align 2
.global	vbar_init
vbar_init:
    ldr x0, =vbar_baseaddr
    msr VBAR_EL1, x0
    //br x30
    ret


// See https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception?lang=en

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
        b dummy_handler

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
        b userToKernel          //Important one for A1
    
    .balign 0x80 
    lower_el_AARCH64_IRQ:
        b dummy_handler

    .balign 0x80 
    lower_el_AARCH64_FIQ:
        b dummy_handler

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
      

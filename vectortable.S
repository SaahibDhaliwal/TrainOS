.text
.align 2
.global	vbar_init
vbar_init:
    ldr x0, =vbar_baseaddr
    msr VBAR_EL1, x0
    //br x30
    ret



// See https://developer.arm.com/documentation/102412/0103/Handling-exceptions/Taking-an-exception?lang=en

.align 11 // this is 2048
vbar_baseaddr:
    same_el_inEL0_SYNC:
        bl dummy_handler

    .align 7 // this is 128
    same_el_inEL0_IRQ:
        bl dummy_handler

    .align 7 
    same_el_inEL0_FIQ:
        bl dummy_handler

    .align 7 
    same_el_inEL0_SERROR:
        bl dummy_handler

    // ---------------------------

    .align 7 
    same_el_inELX_SYNCH:
        bl dummy_handler

    .align 7 
    same_el_inELX_IRQ:
        bl dummy_handler

    .align 7 
    same_el_inELX_FIQ:
        bl dummy_handler

    .align 7 
    same_el_inELX_SERROR:
        bl dummy_handler

// ---------------------------------
    .align 7 
    lower_el_AARCH64_SYNCH:
        bl user_to_kernel          //Important one for A1

    .align 7 
    lower_el_AARCH64_IRQ:
        bl dummy_handler

    .align 7 
    lower_el_AARCH64_FIQ:
        bl dummy_handler

    .align 7 
    lower_el_AARCH64_SERROR:
        bl dummy_handler

// -----------------------------------
    .align 7 
    lower_el_AARCH32_SYNCH:
        bl dummy_handler

    .align 7 
    lower_el_AARCH32_IRQ:
        bl dummy_handler

    .align 7 
    lower_el_AARCH32_FIQ:
        bl dummy_handler

    .align 7 
    lower_el_AARCH32_SERROR:
        bl dummy_handler
      

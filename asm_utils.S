.text
.align 2

.global Create
Create:
    svc 0
    ret

.global MyTid
MyTid:
    svc 1
    ret

.global MyParentTid
MyParentTid:
    svc 2
    ret

.global Yield
Yield:
    svc 3
    ret

.global Exit
Exit:
    svc 4
    ret

//this should probably live somewhere else
.global get_esr
get_esr:
    mrs x0, ESR_EL1
    ret


.global getRegisterx1, getRegisterx2, getRegisterx3, getRegisterx4, getRegisterx5
.global getRegisterx6, getRegisterx7, getRegisterx8, getRegisterx9, getRegisterx10
.global getRegisterx11, getRegisterx12, getRegisterx13, getRegisterx14, getRegisterx15
.global getRegisterx16, getRegisterx17, getRegisterx18, getRegisterx19, getRegisterx20
.global getRegisterx21, getRegisterx22, getRegisterx23, getRegisterx24, getRegisterx25
.global getRegisterx26, getRegisterx27, getRegisterx28, getRegisterx29, getRegisterx30

.global setRegisterx1, setRegisterx2, setRegisterx3, setRegisterx4, setRegisterx5
.global setRegisterx6, setRegisterx7, setRegisterx8, setRegisterx9, setRegisterx10
.global setRegisterx11, setRegisterx12, setRegisterx13, setRegisterx14, setRegisterx15
.global setRegisterx16, setRegisterx17, setRegisterx18, setRegisterx19, setRegisterx20
.global setRegisterx21, setRegisterx22, setRegisterx23, setRegisterx24, setRegisterx25
.global setRegisterx26, setRegisterx27, setRegisterx28, setRegisterx29, setRegisterx30

// -----------------------------------------------------------------------------
// GET REGISTER FUNCTIONS

getRegisterx1:
    mov x0, x1
    ret

getRegisterx2:
    mov x0, x2
    ret

getRegisterx3:
    mov x0, x3
    ret

getRegisterx4:
    mov x0, x4
    ret

getRegisterx5:
    mov x0, x5
    ret

getRegisterx6:
    mov x0, x6
    ret

getRegisterx7:
    mov x0, x7
    ret

getRegisterx8:
    mov x0, x8
    ret

getRegisterx9:
    mov x0, x9
    ret

getRegisterx10:
    mov x0, x10
    ret

getRegisterx11:
    mov x0, x11
    ret

getRegisterx12:
    mov x0, x12
    ret

getRegisterx13:
    mov x0, x13
    ret

getRegisterx14:
    mov x0, x14
    ret

getRegisterx15:
    mov x0, x15
    ret

getRegisterx16:
    mov x0, x16
    ret

getRegisterx17:
    mov x0, x17
    ret

getRegisterx18:
    mov x0, x18
    ret

getRegisterx19:
    mov x0, x19
    ret

getRegisterx20:
    mov x0, x20
    ret

getRegisterx21:
    mov x0, x21
    ret

getRegisterx22:
    mov x0, x22
    ret

getRegisterx23:
    mov x0, x23
    ret

getRegisterx24:
    mov x0, x24
    ret

getRegisterx25:
    mov x0, x25
    ret

getRegisterx26:
    mov x0, x26
    ret

getRegisterx27:
    mov x0, x27
    ret

getRegisterx28:
    mov x0, x28
    ret

getRegisterx29:
    mov x0, x29
    ret

getRegisterx30:
    mov x0, x30
    ret

//-----------------------------------------------------------------
// SET REGISTER FUNCTIONS


setRegisterx1:
    mov x1, x0
    ret

setRegisterx2:
    mov x2, x0
    ret

setRegisterx3:
    mov x3, x0
    ret

setRegisterx4:
    mov x4, x0
    ret

setRegisterx5:
    mov x5, x0
    ret

setRegisterx6:
    mov x6, x0
    ret

setRegisterx7:
    mov x7, x0
    ret

setRegisterx8:
    mov x8, x0
    ret

setRegisterx9:
    mov x9, x0
    ret

setRegisterx10:
    mov x10, x0
    ret

setRegisterx11:
    mov x11, x0
    ret

setRegisterx12:
    mov x12, x0
    ret

setRegisterx13:
    mov x13, x0
    ret

setRegisterx14:
    mov x14, x0
    ret

setRegisterx15:
    mov x15, x0
    ret

setRegisterx16:
    mov x16, x0
    ret

setRegisterx17:
    mov x17, x0
    ret

setRegisterx18:
    mov x18, x0
    ret

setRegisterx19:
    mov x19, x0
    ret

setRegisterx20:
    mov x20, x0
    ret

setRegisterx21:
    mov x21, x0
    ret

setRegisterx22:
    mov x22, x0
    ret

setRegisterx23:
    mov x23, x0
    ret

setRegisterx24:
    mov x24, x0
    ret

setRegisterx25:
    mov x25, x0
    ret

setRegisterx26:
    mov x26, x0
    ret

setRegisterx27:
    mov x27, x0
    ret

setRegisterx28:
    mov x28, x0
    ret

setRegisterx29:
    mov x29, x0
    ret

setRegisterx30:
    mov x30, x0
    ret

#include "protocols/generic_protocol.h"

#include "rpi.h"
#include "sys_call_stubs.h"

int CharReply(int clientTid, char reply) {
    int res = sys::Reply(clientTid, &reply, 1);

    if (res < 0) {
        switch (res) {
            case -1:
                uart_printf(CONSOLE, "[ERROR]: tid %u is not a valid task id\n\r", clientTid);
                break;
            case -2:
                uart_printf(CONSOLE, "[ERROR]: tid %u was not waiting for a reply\n\r", clientTid);
                break;
            default:
                uart_printf(CONSOLE, "[ERROR]: Reply(%u) returned unexpected code %d\n\r", clientTid, res);
                break;
        }
    }
    return res;
}
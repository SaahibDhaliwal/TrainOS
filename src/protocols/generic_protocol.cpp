
#include "protocols/generic_protocol.h"

#include "rpi.h"
#include "sys_call_stubs.h"

void handleSendResponse(int res, int clientTid) {
    if (res < 0) {
        switch (res) {
            case -1:
                uart_printf(CONSOLE, "[ERROR]: tid %u is not a valid task id\r\n", clientTid);
                break;
            case -2:
                uart_printf(CONSOLE, "[ERROR]: tid %u was not waiting for a reply\r\n", clientTid);
                break;
            default:
                uart_printf(CONSOLE, "[ERROR]: Reply(%u) returned unexpected code %d\r\n", clientTid, res);
                break;
        }
    }
}

int emptySend(int clientTid) {
    int res = sys::Send(clientTid, nullptr, 0, nullptr, 0);
    handleSendResponse(res, clientTid);
    return res;
}

int charReply(int clientTid, char reply) {
    int res = sys::Reply(clientTid, &reply, 1);
    handleSendResponse(res, clientTid);
    return res;
}

int uIntReply(int clientTid, uint64_t reply) {
    char buf[21];  // max digits is 20
    ui2a(reply, 10, buf);
    int res = sys::Reply(clientTid, buf, strlen(buf) + 1);
    handleSendResponse(res, clientTid);
    return res;
}
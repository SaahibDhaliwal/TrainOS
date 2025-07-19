
#include "generic_protocol.h"

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

int charSend(int clientTid, const char msg) {
    int res = sys::Send(clientTid, &msg, 1, nullptr, 0);
    handleSendResponse(res, clientTid);
    return res;
}

int emptyReply(int clientTid) {
    int res = sys::Reply(clientTid, nullptr, 0);
    handleSendResponse(res, clientTid);
    return res;
}

int emptyReceive(uint32_t* outSenderTid) {
    int res = sys::Receive(outSenderTid, nullptr, 0);
    return res;
}

uint64_t uIntReceive(uint32_t* outSenderTid) {
    char receiveBuffer[21] = {0};  // max digits is 20
    int res = sys::Receive(outSenderTid, receiveBuffer, 21);
    if (res < 0) return -1;
    unsigned int result = 0;
    a2ui(receiveBuffer, 10, &result);
    return result;
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

int intReply(int clientTid, int64_t reply) {
    char buf[21];  // max digits is 20
    i2a(reply, buf);
    int res = sys::Reply(clientTid, buf, strlen(buf) + 1);
    handleSendResponse(res, clientTid);
    return res;
}
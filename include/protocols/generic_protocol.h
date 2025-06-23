#ifndef __GENERIC_PROTOCOL__
#define __GENERIC_PROTOCOL__

#include <stdint.h>
int charSend(int clientTid, const char msg);
int emptySend(int clientTid);

int charReply(int clientTid, char reply);
int emptyReply(int clientTid);
int emptyReceive(uint32_t* outSenderTid);
int uIntReply(int clientTid, uint64_t reply);
int intReply(int clientTid, int64_t reply);

void handleSendResponse(int res, int clientTid);

#endif
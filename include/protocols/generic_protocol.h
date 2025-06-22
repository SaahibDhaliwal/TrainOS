#ifndef __GENERIC_PROTOCOL__
#define __GENERIC_PROTOCOL__

#include <cstdint>
int charSend(int clientTid, const char msg);
int emptySend(int clientTid);

int charReply(int clientTid, char reply);
int emptyReply(int clientTid);
int emptyReceive(int* outSenderTid);
int uIntReply(int clientTid, uint64_t reply);
int intReply(int clientTid, int64_t reply);

#endif
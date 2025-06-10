#ifndef __GENERIC_PROTOCOL__
#define __GENERIC_PROTOCOL__

#include <cstdint>
int charReply(int clientTid, char reply);
int emptySend(int clientTid);
int uIntReply(int clientTid, uint64_t reply);

#endif
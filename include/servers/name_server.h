#ifndef __NAME_SERVER__
#define __NAME_SERVER__

#include <stdint.h>

#include "config.h"

struct NameTid {
    char name[Config::MAX_MESSAGE_LENGTH];
    int tid;
};

void NameServer(void);

#endif /* name_server.h */
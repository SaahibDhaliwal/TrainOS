#include "sys_call_stubs.h"

#include "config.h"

int RegisterAs(const char *name) {
    // send to name server
    char sendBuf[Config::MAX_MESSAGE_LENGTH];
    char replyBuf[Config::MAX_MESSAGE_LENGTH];

    sendBuf[0] = 'r';
    strcpy(sendBuf + 1, name);

    int response = Send(Config::NAME_SERVER_TID, sendBuf, strlen(name) + 2, replyBuf, Config::MAX_MESSAGE_LENGTH);
    if (response < 0) {
        return -1;
    }
    return 0;
}

int WhoIs(const char *name) {
    char sendBuf[Config::MAX_MESSAGE_LENGTH];
    char replyBuf[Config::MAX_MESSAGE_LENGTH];

    sendBuf[0] = 'w';
    strcpy(sendBuf + 1, name);

    int rc = Send(Config::NAME_SERVER_TID, sendBuf, strlen(name) + 2, replyBuf, Config::MAX_MESSAGE_LENGTH);
    if (rc < 0) {
        return -1;
    }

    unsigned int tid;
    a2ui(replyBuf, 10, &tid);
    return tid;
}
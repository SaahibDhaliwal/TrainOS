#include "servers/name_server.h"

#include "sys_call_stubs.h"

void NameServer() {
    NameTid nameTidPairs[Config::NAME_SERVER_CAPACITY];
    int head = 0;

    for (int i = 0; i < Config::NAME_SERVER_CAPACITY; i++) {
        nameTidPairs[i].name[0] = '\0';
        nameTidPairs[i].tid = -1;
    }

    for (;;) {
        uint32_t senderTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];

        uint32_t messageLen = Receive(&senderTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH);

        switch (receiveBuffer[0]) {
            case 'r': {
                const char* name = receiveBuffer + 1;

                if (head < Config::NAME_SERVER_CAPACITY) {
                    strncpy(nameTidPairs[head].name, name, Config::MAX_MESSAGE_LENGTH - 1);
                    nameTidPairs[head].name[Config::MAX_MESSAGE_LENGTH - 1] = '\0';
                    nameTidPairs[head].tid = senderTid;
                    head++;
                    Reply(senderTid, "0", 2);
                } else {
                    Reply(senderTid, "-1", 3);
                }

                break;
            }
            case 'w': {
                const char* name = receiveBuffer + 1;
                bool found = false;
                unsigned int foundTid = 0;

                for (int i = 0; i < head; i++) {
                    if (strcmp(name, nameTidPairs[i].name) == 0) {
                        char tmpBuffer[Config::MAX_MESSAGE_LENGTH];
                        ui2a(nameTidPairs[i].tid, 10, tmpBuffer);
                        Reply(senderTid, tmpBuffer, strlen(tmpBuffer) + 1);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    Reply(senderTid, "-1", 3);
                }

                break;
            }
            default:
                break;
        }
    }

    Exit();
}
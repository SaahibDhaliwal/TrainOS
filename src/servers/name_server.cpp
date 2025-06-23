#include "name_server.h"

#include "generic_protocol.h"
#include "name_server_protocol.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "test_utils.h"

using namespace name_server;

void NameServer() {
    NameTid nameTidPairs[Config::NAME_SERVER_CAPACITY];
    int head = 0;

    for (int i = 0; i < Config::NAME_SERVER_CAPACITY; i++) {
        nameTidPairs[i].name[0] = '\0';
        nameTidPairs[i].tid = -1;
    }

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH];
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';

        Command command = commandFromByte(receiveBuffer[0]);

        switch (command) {
            case Command::REGISTER: {
                const char* name = receiveBuffer + 1;

                if (head < Config::NAME_SERVER_CAPACITY) {
                    strncpy(nameTidPairs[head].name, name, Config::MAX_MESSAGE_LENGTH - 1);
                    nameTidPairs[head].name[Config::MAX_MESSAGE_LENGTH - 1] = '\0';
                    nameTidPairs[head].tid = clientTid;
                    head++;
                    charReply(clientTid, toByte(Reply::SUCCESS));
                } else {
                    charReply(clientTid, toByte(Reply::FAILURE));
                }

                break;
            }
            case Command::WHO_IS: {
                const char* name = receiveBuffer + 1;
                bool found = false;

                for (int i = 0; i < head; i++) {
                    if (strcmp(name, nameTidPairs[i].name) == 0) {
                        char tmpBuffer[4];
                        ui2a(nameTidPairs[i].tid, 10, tmpBuffer);

                        sys::Reply(clientTid, tmpBuffer, 4);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    charReply(clientTid, toByte(Reply::FAILURE));
                }

                break;
            }
            case Command::KILL: {
                emptyReply(clientTid);
                sys::Exit();
            }
            default: {
                ASSERT(0, "[Name Server]: Unknown Command!\r\n");
                break;
            }
        }
    }

    sys::Exit();
}
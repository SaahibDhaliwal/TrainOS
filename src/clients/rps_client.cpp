#include "protocols/generic_protocol.h"
#include "protocols/ns_protocol.h"
#include "protocols/rps_protocol.h"
#include "rpi.h"
#include "servers/rps_server.h"
#include "sys_call_stubs.h"
#include "timer.h"

using namespace rps;

void RPS_Fixed_Client() {
    int serverTid = name_server::WhoIs(RPS_SERVER_NAME);

    uint32_t parentTid = 0;
    char parentMsg[4] = {0};
    sys::Receive(&parentTid, parentMsg, 3);
    charReply(parentTid, '0');

    bool forced_hand = (parentMsg[0] == 'F');
    int numPlays = a2d(parentMsg[2]);

    if (Signup(serverTid) < 0) {
        uart_printf(CONSOLE, "[Client %u ]: SIGNUP failed\n\r", sys::MyTid());
        sys::Exit();
    }
    uart_printf(CONSOLE, "[Client %u ]: Signed up!\n\r", sys::MyTid());

    for (int i = 0; i < numPlays; ++i) {
        int rngValue;
        if (forced_hand) {
            rngValue = a2d(parentMsg[1]);
        } else {
            rngValue = ((timerGet() % 10) % 3) + 1;
        }

        Move move = static_cast<Move>(rngValue);
        Reply reply = Play(serverTid, move);
        uart_printf(CONSOLE, "[Client: %u ]: Played %s Result: %s\n\r", sys::MyTid(), moveToString(move),
                    replyToString(reply));
    }

    if (Quit(serverTid) < 0) {
        uart_printf(CONSOLE, "[Client %u ]: Quit failed\n\r", sys::MyTid());
    } else {
        uart_printf(CONSOLE, "[Client %u ]: Successfully quit!\n\r", sys::MyTid());
    }
    sys::Exit();
}

void RPS_Random_Client() {
    int serverTid = name_server::WhoIs(RPS_SERVER_NAME);
    if (serverTid < 0) {
        uart_printf(CONSOLE, "[Client %u]: Could not find  server\n\r", sys::MyTid());
        sys::Exit();
    }

    if (Signup(serverTid) < 0) {
        uart_printf(CONSOLE, "[Client %u]: Signup failed!\n\r", sys::MyTid());
        sys::Exit();
    }
    uart_printf(CONSOLE, "[Client %u]: Signed up!\n\r", sys::MyTid());

    int numPlays = 3;
    bool needResignup = false;

    for (int i = 0; i < numPlays; i++) {
        int rngValue = ((timerGet() % 10) % 3) + 1;
        Move move = static_cast<Move>(rngValue);

        Reply reply = Play(serverTid, move);

        uart_printf(CONSOLE, "[Client %u]: Played: %s, Result: %s\n\r", sys::MyTid(), moveToString(move),
                    replyToString(reply));

        if (reply == Reply::OPPONENT_QUIT) {
            needResignup = true;
            break;
        }
    }

    if (needResignup) {
        uart_printf(CONSOLE, "[Client: %u ]: My opponent quit. Signing up again...\n\r", sys::MyTid());
    }

    if (Quit(serverTid) < 0) {
        uart_printf(CONSOLE, "[Client %u]: QUIT failed\n\r", sys::MyTid());
    } else {
        uart_printf(CONSOLE, "[Client %u]: Successfully quit\n\r", sys::MyTid());
    }
    sys::Exit();
}

void RPS_Random_Client2() {
    int serverTid = name_server::WhoIs(RPS_SERVER_NAME);
    if (serverTid < 0) {
        uart_printf(CONSOLE, "[Client %u]: Could not find  server\n\r", sys::MyTid());
        sys::Exit();
    }

    int numPlays = 7;
    int cycles = 2;

    for (int cycle = 0; cycle < cycles; ++cycle) {
        if (Signup(serverTid) < 0) {
            uart_printf(CONSOLE, "[Client %u]: Signup failed!\n\r", sys::MyTid());
            sys::Exit();
        }
        uart_printf(CONSOLE, "[Client %u]: Signed up!\n\r", sys::MyTid());

        for (int i = 0; i < numPlays; ++i) {
            int rngVal = static_cast<int>((timerGet() % 10) % 3) + 1;
            Move move = static_cast<Move>(rngVal);

            Reply reply = Play(serverTid, move);
            uart_printf(CONSOLE, "[Client %u]: Played: %s, Result: %s \n\r", sys::MyTid(), moveToString(move),
                        replyToString(reply));

            if (reply == Reply::OPPONENT_QUIT) {
                uart_printf(CONSOLE, "[Client: %u ]: My opponent quit. Signing up again...\n\r", sys::MyTid());
                break;
            }
        }
    }

    if (Quit(serverTid) < 0) {
        uart_printf(CONSOLE, "[Client %u]: QUIT failed\n\r", sys::MyTid());
    } else {
        uart_printf(CONSOLE, "[Client %u]: Successfully quit\n\r", sys::MyTid());
    }
    sys::Exit();
}
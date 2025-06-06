#include "rpi.h"
#include "servers/name_server.h"
#include "sys_call_stubs.h"

void RPS_Client() {
    int serverTID = WhoIs("rps_server");
    char msg[] = "SIGNUP";
    int quitflag = 1;

    for (;;) {
        char reply[Config::MAX_MESSAGE_LENGTH];
        if (quitflag) {
            Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);
            quitflag = 0;
        }

        char msg2[] = "PLAY SCISSORS";
        Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
        uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,        Result: %s\n\r", MyTid(), reply);
        if (!strcmp(reply, "Sorry, your opponent quit.")) {
            quitflag = 1;
        }

        char msg3[] = "QUIT";
        Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
        uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit!\n\r", MyTid());
        Exit();
    }
}

void RPS_Fixed_Client() {
    int serverTID = WhoIs("rps_server");
    uint32_t clientTID = 0;
    char parentMsg[Config::MAX_MESSAGE_LENGTH] = {0};

    int msgsize = Receive(&clientTID, parentMsg, Config::MAX_MESSAGE_LENGTH);
    Reply(clientTID, "0", Config::MAX_MESSAGE_LENGTH);

    bool forced_hand = 0;
    int num_plays = Config::EXPERIMENT_COUNT;
    if (parentMsg[0] == 'F') {
        forced_hand = true;
    }
    num_plays = a2d(parentMsg[2]);

    char msg[] = "SIGNUP";
    char reply[Config::MAX_MESSAGE_LENGTH];
    int replylen = Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);

    for (int i = 0; i < num_plays; i++) {
        int rngValue = -1;

        if (forced_hand) {
            rngValue = a2d(parentMsg[1]);
        } else {
            rngValue = ((get_timer() % 10) % 3) + 1;  // note: this makes rock 4/10 chance vs 3/10 for others
        }
        switch (rngValue) {
            case 1: {
                char msg2[] = "PLAY ROCK";
                Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played ROCK,           Result: %s\n\r", MyTid(), reply);
            } break;
            case 2: {
                char msg2[] = "PLAY PAPER";
                Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,          Result: %s\n\r", MyTid(), reply);
            } break;
            case 3: {
                char msg2[] = "PLAY SCISSORS";
                Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,       Result: %s\n\r", MyTid(), reply);
            } break;
        }
    }
    // Yield();
    char msg3[] = "QUIT";
    Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Successfully quit!\n\r", MyTid());
    Exit();
}

void RPS_Random_Client2() {
    int serverTID = WhoIs("rps_server");
    int num_plays = 7;
    char msg[] = "SIGNUP";
    int quitflag = 1;

    for (int j = 0; j < 2; j++) {
        char reply[Config::MAX_MESSAGE_LENGTH] = {0};
        if (quitflag) {
            Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);
            quitflag = 0;
        }

        for (int i = 0; i < num_plays; i++) {
            int rngValue = ((get_timer() % 10) % 3) + 1;

            if (rngValue == 1) {
                char msg2[] = "PLAY ROCK";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played ROCK,           Result: %s\n\r", MyTid(), reply);
                // if (!strcmp(reply, "Sorry, your opponent quit.")) {  // weird compiler behaviour if this is
                // outside?
                //     quitflag = 1;
                // }
            } else if (rngValue == 2) {
                char msg2[] = "PLAY PAPER";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,          Result: %s\n\r", MyTid(), reply);
                // if (!strcmp(reply, "Sorry, your opponent quit.")) {
                //     quitflag = 1;
                // }

            } else if (rngValue == 3) {
                char msg2[] = "PLAY SCISSORS";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,       Result: %s\n\r", MyTid(), reply);
                // if (!strcmp(reply, "Sorry, your opponent quit.")) {
                //     quitflag = 1;
                // }
            } else {
                uart_printf(CONSOLE, "BAD RNG");
            }

            if (!strcmp(reply, "Sorry, your opponent quit.")) {
                quitflag = 1;
                break;
            }
        }
        // int quitRNG = (get_timer() % 100);
        // // uart_printf(CONSOLE, "quitrng:%u ", quitRNG);
        // if (quitRNG <= 10) {
        //     char msg3[] = "QUIT";
        //     Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
        //     uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit! %u\n\r", MyTid(), quitRNG);
        //     Exit();
        // }
        if (quitflag) {
            uart_printf(CONSOLE, "[Client: %u ]: My opponent quit. Signing up again...\n\r", MyTid());
            // break;
        }
    }
    char reply[Config::MAX_MESSAGE_LENGTH] = {0};
    char msg4[] = "QUIT";
    Send(serverTID, msg4, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit!\n\r", MyTid());
    Exit();
}

void RPS_Random_Client() {
    int serverTID = WhoIs("rps_server");
    //  uint32_t clientTID = 0;
    // char parentMsg[Config::MAX_MESSAGE_LENGTH] = {0};

    // int msgsize = Receive(&clientTID, parentMsg, Config::MAX_MESSAGE_LENGTH);
    // Reply(clientTID, "0", Config::MAX_MESSAGE_LENGTH);

    // bool forced_hand = 0;
    int num_plays = 3;
    // if(parentMsg[0] == 'F'){
    //     forced_hand = true;
    // }
    // num_plays = a2d(parentMsg[2]);

    char msg[] = "SIGNUP";
    int quitflag = 1;

    // for (int j = 0; j < 2; j++) {
    char reply[Config::MAX_MESSAGE_LENGTH] = {0};
    if (quitflag) {
        Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
        uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);
        quitflag = 0;
    }

    for (int i = 0; i < num_plays; i++) {
        int rngValue = ((get_timer() % 10) % 3) + 1;

        if (rngValue == 1) {
            char msg2[] = "PLAY ROCK";
            Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Played ROCK,           Result: %s\n\r", MyTid(), reply);
            // if (!strcmp(reply, "Sorry, your opponent quit.")) {  // weird compiler behaviour if this is outside
            //     quitflag = 1;
            // }
        } else if (rngValue == 2) {
            char msg2[] = "PLAY PAPER";
            Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,          Result: %s\n\r", MyTid(), reply);
            // if (!strcmp(reply, "Sorry, your opponent quit.")) {
            //     quitflag = 1;
            // }

        } else if (rngValue == 3) {
            char msg2[] = "PLAY SCISSORS";
            Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,       Result: %s\n\r", MyTid(), reply);
            // if (!strcmp(reply, "Sorry, your opponent quit.")) {
            //     quitflag = 1;
            // }
        } else {
            uart_printf(CONSOLE, "BAD RNG");
        }

        if (!strcmp(reply, "Sorry, your opponent quit.")) {
            quitflag = 1;
            break;
        }
    }
    // int quitRNG = (get_timer() % 100);
    // // uart_printf(CONSOLE, "quitrng:%u ", quitRNG);
    // if (quitRNG <= 10) {
    //     char msg3[] = "QUIT";
    //     Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    //     uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit! %u\n\r", MyTid(), quitRNG);
    //     Exit();
    // }
    if (quitflag) {
        uart_printf(CONSOLE, "[Client: %u ]: My opponent quit. Signing up again...\n\r", MyTid());
        // break;
    }
    // }
    // char reply[Config::MAX_MESSAGE_LENGTH] = {0};
    char msg4[] = "QUIT";
    Send(serverTID, msg4, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit!\n\r", MyTid());
    Exit();
}
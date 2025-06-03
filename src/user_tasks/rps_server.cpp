#include <string.h>

#include <cstdint>
#include <map>

#include "user_tasks.h"

// #include "fixed_map.h"
#include "name_server.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "util.h"

void RPS_Server();
void RPS_Fixed_Client();
void RPS_Random_Client();
void RPS_Random_Client2();

void RPSFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(9, &NameServer));
    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\n\r", Create(8, &RPS_Server));

    uart_printf(CONSOLE, "\n\r------  Starting first test: Both tie with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);
    char reply = 0;
    int player1 = Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    Send(player1, "F11", 3, &reply, 1);
    int player2 = Create(9, &RPS_Fixed_Client);
    Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting second test: Player 1 wins with Rock ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, Rock, 1 iteration
    Send(player1, "F11", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, Scissors, 1 iteration
    Send(player2, "F31", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting third test: Player 1 wins with Paper ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    Send(player1, "F21", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    Send(player2, "F11", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fourth test: Player 1 wins with Scissors ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    // Forced, paper, 1 iteration
    Send(player1, "F31", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    // Forced, rock, 1 iteration
    Send(player2, "F21", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Starting fifth test: Random action choice for five rounds ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Fixed_Client);
    Send(player1, "005", 3, &reply, 1);
    player2 = Create(9, &RPS_Fixed_Client);
    Send(player2, "005", 3, &reply, 1);

    uart_printf(
        CONSOLE,
        "\n\r------  Starting sixth test: Random action choice for three rounds, with three clients () ---------\n\r");
    uart_printf(CONSOLE, "------  Press any key to start ---------\n\r");
    uart_getc(CONSOLE);

    player1 = Create(9, &RPS_Random_Client);
    // Send(player1, "005", 3, &reply, 1);

    player2 = Create(9, &RPS_Random_Client2);
    // Send(player2, "005", 3, &reply, 1);

    int player3 = Create(9, &RPS_Random_Client);
    // Send(player3, "005", 3, &reply, 1);

    uart_printf(CONSOLE, "\n\r------  Tests have completed! ---------\n\r");
    // uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(8, &RPS_Fixed_Client));
    // uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(8, &RPS_Fixed_Client));

    // printf();

    Exit();
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
    Yield();
    char msg3[] = "QUIT";
    Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Successfully quit!\n\r", MyTid());
    Exit();
}

void RPS_Random_Client2() {
    int serverTID = WhoIs("rps_server");
    //  uint32_t clientTID = 0;
    // char parentMsg[Config::MAX_MESSAGE_LENGTH] = {0};

    // int msgsize = Receive(&clientTID, parentMsg, Config::MAX_MESSAGE_LENGTH);
    // Reply(clientTID, "0", Config::MAX_MESSAGE_LENGTH);

    // bool forced_hand = 0;
    int num_plays = 7;
    // if(parentMsg[0] == 'F'){
    //     forced_hand = true;
    // }
    // num_plays = a2d(parentMsg[2]);

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
                if (!strcmp(reply, "Sorry, your partner quit.")) {  // weird compiler behaviour if this is outside
                    quitflag = 1;
                }
            } else if (rngValue == 2) {
                char msg2[] = "PLAY PAPER";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,          Result: %s\n\r", MyTid(), reply);
                if (!strcmp(reply, "Sorry, your partner quit.")) {
                    quitflag = 1;
                }

            } else if (rngValue == 3) {
                char msg2[] = "PLAY SCISSORS";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,       Result: %s\n\r", MyTid(), reply);
                if (!strcmp(reply, "Sorry, your partner quit.")) {
                    quitflag = 1;
                }
            } else {
                uart_printf(CONSOLE, "BAD RNG");
            }

            if (!strcmp(reply, "Sorry, your partner quit.")) {
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
            uart_printf(CONSOLE, "[Client: %u ]: My partner quit. Signing up again...\n\r", MyTid());
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
            if (!strcmp(reply, "Sorry, your partner quit.")) {  // weird compiler behaviour if this is outside
                quitflag = 1;
            }
        } else if (rngValue == 2) {
            char msg2[] = "PLAY PAPER";
            Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,          Result: %s\n\r", MyTid(), reply);
            if (!strcmp(reply, "Sorry, your partner quit.")) {
                quitflag = 1;
            }

        } else if (rngValue == 3) {
            char msg2[] = "PLAY SCISSORS";
            Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,       Result: %s\n\r", MyTid(), reply);
            if (!strcmp(reply, "Sorry, your partner quit.")) {
                quitflag = 1;
            }
        } else {
            uart_printf(CONSOLE, "BAD RNG");
        }

        if (!strcmp(reply, "Sorry, your partner quit.")) {
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
        uart_printf(CONSOLE, "[Client: %u ]: My partner quit. Signing up again...\n\r", MyTid());
        // break;
    }
    // }
    // char reply[Config::MAX_MESSAGE_LENGTH] = {0};
    char msg4[] = "QUIT";
    Send(serverTID, msg4, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit!\n\r", MyTid());
    Exit();
}

class rps_player {
   public:
    rps_player* next = nullptr;
    uint32_t TID = 0;
    rps_player* paired_with = nullptr;
    // uint32_t slabIdx;

    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

    // no oop bc lazy :(
    int waiting_on_pair = 0;
    int quit = 0;
    rps_player(uint64_t newTID = 0) {
        TID = newTID;
    }
    uint64_t getTid() {
        return TID;
    }
    rps_player* getPair() {
        return paired_with;
    }
    void setTid(uint64_t id) {
        TID = id;
    }
    void setPair(rps_player* id) {
        paired_with = id;
    }
};

void clearPlayer(rps_player* p) {
    p->waiting_on_pair = 0;
    p->quit = 0;
    p->TID = 0;
    p->paired_with = nullptr;
}

void RPS_Server() {
    int registerReturn = RegisterAs("rps_server");
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER \n\r");
        Exit();
    }

    // signup queue, popped two at a time
    const int MAX_PLAYERS = 64;
    Queue<rps_player> player_queue;
    rps_player playerSlabs[MAX_PLAYERS];
    Stack<rps_player> freelist;

    // initialize our structs
    for (int i = 0; i < MAX_PLAYERS; i += 1) {
        freelist.push(&playerSlabs[i]);
    }

    for (;;) {
        uint32_t clientTID = 0;
        char msg[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgsize = Receive(&clientTID, msg, Config::MAX_MESSAGE_LENGTH);

        uart_printf(CONSOLE, "[RPS Server]: Request from TID: %u: '%s' \n\r", clientTID, msg);
        // this will parse the message we receive
        //  split along the first space, if found
        char command[64] = {0};
        char param[64] = {0};
        int msg_parse = 0;
        int tracker = 0;
        for (int i = 0; i < msgsize; i++) {
            if (!msg_parse) {
                if (msg[i] != ' ') {
                    command[i] = msg[i];
                } else {
                    command[i] = '\0';
                    msg_parse = 1;
                }
            } else {
                param[tracker] = msg[i];
                tracker++;
            }
        }
        param[tracker] = '\0';

        // parse info
        if (!strcmp(command, "SIGNUP")) {  // if match, strcmp equals zero
            // once two are on queue, reply and ask for first play
            // player_queue.push(&client);
            if (freelist.empty()) {
                uart_printf(CONSOLE, "No more space in free list!!\n\r");
            }
            // get new rps player from slab
            rps_player* newplayer = freelist.pop();
            clearPlayer(newplayer);
            newplayer->paired_with = 0;

            newplayer->setTid(clientTID);
            // put them on the end of the queue
            player_queue.push(newplayer);

            if (player_queue.tally() < 2) {
                // uart_printf(CONSOLE, "Not enough people, TID: %u\r\n", clientTID);
                continue;
            }

            char signup_msg[] = "What is your play?";

            rps_player* player_one = player_queue.pop();
            Reply(player_one->getTid(), signup_msg, Config::MAX_MESSAGE_LENGTH);
            rps_player* player_two = player_queue.pop();
            Reply(player_two->getTid(), signup_msg, Config::MAX_MESSAGE_LENGTH);

            uart_printf(CONSOLE, "[RPS Server]: TID(%u) is paired with TID(%u)\n\r", player_one->getTid(),
                        player_two->getTid());

            player_one->setPair(player_two);
            player_two->setPair(player_one);

            // push both of them on the map of pairs

        } else if (!strcmp(command, "PLAY")) {
            int j = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                j = i;
                if (playerSlabs[i].getTid() == clientTID) {
                    rps_player* partner = playerSlabs[i].getPair();
                    if (!partner) {
                        uart_printf(CONSOLE, "ERROR: Trying to play without signing up!\n\r");
                        break;
                    }

                    if (playerSlabs[i].quit) {
                        // partner sets this to quit upon them quitting
                        // If we put them back on the queue, our logic flow would get wayyy to complex
                        // uart_printf(CONSOLE, "uhoh, partnerTID: %d", playerSlabs[i].paired_with->TID);

                        char msg[] = "Sorry, your partner quit.";
                        Reply(clientTID, msg, Config::MAX_MESSAGE_LENGTH);

                        freelist.push(&playerSlabs[i]);
                        // clearPlayer(&playerSlabs[i]);  // let our partner know we quit

                        break;
                    }

                    int clientAction = 0;
                    if (!strcmp(param, "ROCK")) {
                        clientAction = 1;
                    } else if (!strcmp(param, "PAPER")) {
                        clientAction = 2;
                    } else if (!strcmp(param, "SCISSORS")) {
                        clientAction = 3;
                    } else {
                        uart_printf(CONSOLE, "ERROR: NOT A VALID ACTION FROM {ROCK, PAPER, SCISSORS} FROM TID %u: \n\r",
                                    clientTID);
                        uart_printf(CONSOLE, "GOT: %s strcmp: %d\n\r", param, strcmp(param, "SCISSORS"));
                        break;
                    }

                    if (!partner->waiting_on_pair) {
                        // partner is not waiting on me (I have played first)
                        // so I am now waiting for my partner to send their message
                        playerSlabs[i].waiting_on_pair = clientAction;

                    } else {
                        // partner was waiting on me, time to do the game!
                        int partnerAction = partner->waiting_on_pair;
                        const char* winner_msg = "Winner!\0";
                        const char* loser_msg = "Loser!\0";
                        const char* tie_msg = "Tied!\0";

                        if (clientAction == partnerAction) {
                            // tied
                            Reply(clientTID, tie_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), tie_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 1 && partnerAction == 2) {
                            // client loss, partner won (rock loses to paper)
                            Reply(clientTID, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 1 && partnerAction == 3) {
                            // client won, partner loss (rock beats scissors)
                            Reply(clientTID, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), loser_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 2 && partnerAction == 1) {
                            // client won, partner loss (paper beats rock)
                            Reply(clientTID, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), loser_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 2 && partnerAction == 3) {
                            // client loss, partner won (paper loses to scissors)
                            Reply(clientTID, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 3 && partnerAction == 1) {
                            // client loss, partner won (scissors loses to rock)
                            Reply(clientTID, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 3 && partnerAction == 2) {
                            // client won, partner loss (scissors beats paper)
                            Reply(clientTID, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(partner->getTid(), loser_msg, Config::MAX_MESSAGE_LENGTH);
                        }
                        // clear their waiting
                        playerSlabs[i].waiting_on_pair = 0;
                        partner->waiting_on_pair = 0;
                    }
                    // found our desired slab, stop looping through the rest
                    break;
                }
            }

            // worth checking if we made it out
            //  uart_printf(CONSOLE, "ERROR: I VALUE: %u\n\r", j);

        } else if (!strcmp(command, "QUIT")) {
            // remove from our struct
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (playerSlabs[i].getTid() == clientTID) {
                    playerSlabs[i].getPair()->quit = 1;  // let our partner know we quit CRUCIAL
                    freelist.push(&playerSlabs[i]);
                    // clearPlayer(&playerSlabs[i]);

                    char quit_msg[] = "You have quit";
                    Reply(clientTID, quit_msg, Config::MAX_MESSAGE_LENGTH);

                    freelist.push(playerSlabs[i].paired_with);
                    char quit_partner_msg[] = "Sorry, your partner quit.";
                    Reply(playerSlabs[i].paired_with->TID, quit_partner_msg, Config::MAX_MESSAGE_LENGTH);

                    break;
                }
            }
            // uart_printf(CONSOLE, "Error: could not find slab\r\n");

        } else {
            uart_printf(CONSOLE, "That was not a valid command! First word must be {SIGNUP, PLAY, QUIT}\n\r");
        }
    }
    Exit();  // in case we ever somehow break out of the infinite for loop
}
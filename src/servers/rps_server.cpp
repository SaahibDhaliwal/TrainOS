#include "servers/rps_server.h"

#include <string.h>

#include "queue.h"
#include "rpi.h"
#include "servers/name_server.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "user_tasks.h"
#include "util.h"

namespace {
class RpsPlayer : public IntrusiveNode<RpsPlayer> {
   public:
    uint32_t tid = 0;
    RpsPlayer* opponent = nullptr;
    int move = 0;
    bool exited = false;

    void initialize(int newTid) {
        move = 0;
        exited = false;
        tid = newTid;
        opponent = nullptr;
    }
};
}  // namespace

void RPS_Server() {
    int registerReturn = RegisterAs("rps_server");
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME SERVER \n\r");
        Exit();
    }

    // signup queue, popped two at a time
    const int MAX_PLAYERS = 64;
    Queue<RpsPlayer> playerQueue;
    RpsPlayer playerSlabs[MAX_PLAYERS];
    Stack<RpsPlayer> freelist;

    // initialize our structs
    for (int i = 0; i < MAX_PLAYERS; i += 1) {
        freelist.push(&playerSlabs[i]);
    }

    for (;;) {
        uint32_t clientTid = 0;
        char msg[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgsize = Receive(&clientTid, msg, Config::MAX_MESSAGE_LENGTH);

        uart_printf(CONSOLE, "[RPS Server]: Request from tid: %u: '%s' \n\r", clientTid, msg);
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
            // playerQueue.push(&client);
            if (freelist.empty()) {
                uart_printf(CONSOLE, "No more space in free list!!\n\r");
            }
            // get new rps player from slab
            RpsPlayer* newPlayer = freelist.pop();
            newPlayer->initialize(clientTid);

            // put them on the end of the queue
            playerQueue.push(newPlayer);

            if (playerQueue.size() < 2) {
                // uart_printf(CONSOLE, "Not enough people, tid: %u\r\n", clientTid);
                continue;
            }

            char signup_msg[] = "What is your play?";

            RpsPlayer* p1 = playerQueue.pop();
            Reply(p1->tid, signup_msg, Config::MAX_MESSAGE_LENGTH);
            RpsPlayer* p2 = playerQueue.pop();
            Reply(p2->tid, signup_msg, Config::MAX_MESSAGE_LENGTH);

            uart_printf(CONSOLE, "[RPS Server]: tid(%u) is paired with tid(%u)\n\r", p1->tid, p2->tid);

            p1->opponent = p2;
            p2->opponent = p1;

            // push both of them on the map of pairs

        } else if (!strcmp(command, "PLAY")) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (playerSlabs[i].tid == clientTid) {
                    RpsPlayer* opponent = playerSlabs[i].opponent;
                    if (!opponent) {
                        uart_printf(CONSOLE, "ERROR: Trying to play without signing up!\n\r");
                        break;
                    }

                    if (playerSlabs[i].exited) {
                        // opponent sets this to quit upon them quitting
                        // If we put them back on the queue, our logic flow would get way to complex

                        char msg[] = "Sorry, your opponent quit.";
                        Reply(clientTid, msg, Config::MAX_MESSAGE_LENGTH);

                        freelist.push(&playerSlabs[i]);

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
                        uart_printf(CONSOLE, "ERROR: NOT A VALID ACTION FROM {ROCK, PAPER, SCISSORS} FROM tid %u: \n\r",
                                    clientTid);
                        uart_printf(CONSOLE, "GOT: %s strcmp: %d\n\r", param, strcmp(param, "SCISSORS"));
                        break;
                    }

                    if (!opponent->move) {
                        // opponent is not waiting on me (I have played first)
                        // so I am now waiting for my opponent to send their message
                        playerSlabs[i].move = clientAction;

                    } else {
                        // opponent was waiting on me, time to do the game!
                        int opponentAction = opponent->move;
                        const char* winner_msg = "Winner!\0";
                        const char* loser_msg = "Loser!\0";
                        const char* tie_msg = "Tied!\0";

                        if (clientAction == opponentAction) {
                            // tied
                            Reply(clientTid, tie_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, tie_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 1 && opponentAction == 2) {
                            // client loss, opponent won (rock loses to paper)
                            Reply(clientTid, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 1 && opponentAction == 3) {
                            // client won, opponent loss (rock beats scissors)
                            Reply(clientTid, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, loser_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 2 && opponentAction == 1) {
                            // client won, opponent loss (paper beats rock)
                            Reply(clientTid, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, loser_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 2 && opponentAction == 3) {
                            // client loss, opponent won (paper loses to scissors)
                            Reply(clientTid, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 3 && opponentAction == 1) {
                            // client loss, opponent won (scissors loses to rock)
                            Reply(clientTid, loser_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, winner_msg, Config::MAX_MESSAGE_LENGTH);

                        } else if (clientAction == 3 && opponentAction == 2) {
                            // client won, opponent loss (scissors beats paper)
                            Reply(clientTid, winner_msg, Config::MAX_MESSAGE_LENGTH);
                            Reply(opponent->tid, loser_msg, Config::MAX_MESSAGE_LENGTH);
                        }
                        // clear their waiting
                        playerSlabs[i].move = 0;
                        opponent->move = 0;
                    }
                    // found our desired slab, stop looping through the rest
                    break;
                }
            }

        } else if (!strcmp(command, "QUIT")) {
            // remove from our struct
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (playerSlabs[i].tid == clientTid) {
                    playerSlabs[i].opponent->exited = true;  // let our opponent know we quit CRUCIAL
                    freelist.push(&playerSlabs[i]);

                    char quit_msg[] = "You have quit";
                    Reply(clientTid, quit_msg, Config::MAX_MESSAGE_LENGTH);

                    // if our opponent was waiting for us, we need to free them
                    if (playerSlabs[i].opponent->move) {
                        freelist.push(playerSlabs[i].opponent);
                        char quit_opponent_msg[] = "Sorry, your opponent quit.";
                        Reply(playerSlabs[i].opponent->tid, quit_opponent_msg, Config::MAX_MESSAGE_LENGTH);
                    }

                    break;
                }
            }

        } else {
            uart_printf(CONSOLE, "That was not a valid command! First word must be {SIGNUP, PLAY, QUIT}\n\r");
        }
    }
    Exit();  // in case we ever somehow break out of the infinite for loop
}

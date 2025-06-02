#include "user_tasks.h"

#include <string.h>

#include <cstdint>
#include <map>

#include "fixed_map.h"
#include "name_server.h"
#include "queue.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "util.h"

void FirstUserTask() {
    //  Assumes FirstUserTask is at Priority 2
    const uint64_t lowerPrio = 2 - 1;
    const uint64_t higherPrio = 2 + 1;

    // create two tasks at a lower priority
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));

    // // create two tasks at a higher priority
    uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, &TestTask));

    uart_printf(CONSOLE, "FirstUserTask: exiting\n\r");
    Exit();
}

void TestTask() {
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
    Yield();
    uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
    Exit();
}

void IdleTask() {
    while (true) {
        asm volatile("wfe");
    }
}

void SenderTask() {
    // RegisterAs("sender");

    // //this won't work as intended
    // for(;;){
    //     int receiverTID = WhoIs("receiver");
    //     if(receiverTID != -1) break;
    //     Yield();
    // }
    // int receiverTID = WhoIs("receiver");
    char* msg =
        "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum "
        "sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies "
        "nec, pellentesque eu, pretium quis,.";

    int start = 0;
    int end = 0;
    start = get_timer();
    end = get_timer();
    // uart_printf(CONSOLE, "first clock diff: %u \n\r", end - start);

    
    char reply[Config::MAX_MESSAGE_LENGTH];
    start = get_timer();
    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        uart_printf(CONSOLE, "clock time diff: %u \n\r", end - start);
        int replylen = Send(4, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    }
    end = get_timer();
    Exit();
}

void ReceiverTask() {
    // RegisterAs("receiver");

    // //this won't work as intended
    // for(;;){
    //     int receiverTID = WhoIs("sender");
    //     if(receiverTID != -1) break;
    //     Yield();
    // }
    // int receiverTID = WhoIs("receiver");

    char reply[Config::MAX_MESSAGE_LENGTH];

    for (int i = 0; i < Config::EXPERIMENT_COUNT; i++) {
        uint32_t sender;
        char buffer[Config::MAX_MESSAGE_LENGTH];

        int msgLen = Receive(&sender, buffer, Config::MAX_MESSAGE_LENGTH);
        // uart_printf(CONSOLE, "Received Message! \n\r", buffer);
        Reply(sender, "Got Message", Config::MAX_MESSAGE_LENGTH);
        // uart_printf(CONSOLE, "Sent Reply! \n\r");
    }
    Exit();
}

void PerformanceMeasurement() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(10, &NameServer));

    uart_printf(CONSOLE, "[First Task]: Created Sender: %u\n\r", Create(9, &SenderTask));
    uart_printf(CONSOLE, "[First Task]: Created Receiver: %u\n\r", Create(9, &ReceiverTask));
    Exit();
}

void RPS_Server();
void RPS_Client();

void RPSFirstUserTask() {
    uart_printf(CONSOLE, "[First Task]: Created NameServer: %u\n\r", Create(9, &NameServer));

    uart_printf(CONSOLE, "[First Task]: Created RPS Server: %u\n\r", Create(9, &RPS_Server));

    uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client));

    uart_printf(CONSOLE, "[First Task]: Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client));

    Exit();
}

void RPS_Client() {

    int serverTID = WhoIs("rps_server");
    char msg[] = "SIGNUP";
    // char reply[Config::MAX_MESSAGE_LENGTH];
    // int replylen = Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    // uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);

    // for(int i = 0; i< Config::EXPERIMENT_COUNT; i++){
    //     int rngValue = ((get_timer() % 10) % 3) + 1;
    //     switch (rngValue) {
    //         case 1: {
    //             char msg2[] = "PLAY ROCK";
    //             Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    //             uart_printf(CONSOLE, "[Client: %u ]: Played ROCK,        Result: %s\n\r", MyTid(), reply);
    //         } break;
    //         case 2: {
    //             char msg2[] = "PLAY PAPER";
    //             Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    //             uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,       Result: %s\n\r", MyTid(), reply);
    //         } break;
    //         case 3: {
    //             char msg2[] = "PLAY SCISSORS";
    //             Send(serverTID, msg2, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    //             uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,    Result: %s\n\r", MyTid(), reply);
    //         } break;
    //     }
    // }

    //         char msg3[] = "QUIT";
    //         Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    //         uart_printf(CONSOLE, "[Client: %u ]: Successfully quit!\n\r", MyTid());
    //         Exit();

    // }
    int quitflag = 1;

    for(;;){
        char reply[Config::MAX_MESSAGE_LENGTH];
        if (quitflag){
            Send(serverTID, msg, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Signed up! Server reply: '%s'\n\r", MyTid(), reply);
            quitflag = 0;
        }
        
    
        for(int i = 0; i < Config::EXPERIMENT_COUNT; i++){
            int rngValue = ((get_timer() % 10) % 3) + 1;

            if(rngValue == 1){
                char msg2[] = "PLAY ROCK";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played ROCK,        Result: %s\n\r", MyTid(), reply);
                if (!strcmp(reply, "Sorry, your partner quit.")){ //weird compiler behaviour if this is outside
                    quitflag = 1;
                }
            } else if (rngValue == 2){
                char msg2[] = "PLAY PAPER";
                Send(serverTID, msg2, strlen(msg2) + 1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played PAPER,       Result: %s\n\r", MyTid(), reply);
                
            } else if (rngValue == 3){
                char msg2[] = "PLAY SCISSORS";
                Send(serverTID, msg2, strlen(msg2)+1, reply, Config::MAX_MESSAGE_LENGTH);
                uart_printf(CONSOLE, "[Client: %u ]: Played SCISSORS,    Result: %s\n\r", MyTid(), reply);
                if (!strcmp(reply, "Sorry, your partner quit.")){
                    quitflag = 1;
                }
            } else {
                uart_printf(CONSOLE, "BAD RNG");
            }

            if (!strcmp(reply, "Sorry, your partner quit.")){
                    break;
            }

            if (quitflag){
                break;
            }
       
        }
        int quitRNG = (get_timer() % 100);
        // uart_printf(CONSOLE, "quitrng:%u ", quitRNG);
        if(quitRNG <= 10){
            char msg3[] = "QUIT";
            Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
            uart_printf(CONSOLE, "[Client: %u ]: Successfully Quit!\n\r", MyTid());
            Exit();
        }
        if (quitflag){
            uart_printf(CONSOLE, "[Client: %u ]: My partner quit. Signing up again...\n\r", MyTid());
            // break;
        }

    }

    // char msg2[] = "PLAY ROCK";
    //  uart_printf(CONSOLE, "[Task: %u] Played: \n\r", MyTid());

    // strcpy(msg, "PLAY ROCK");
    // char msg3[] = "QUIT";
    // Send(serverTID, msg3, Config::MAX_MESSAGE_LENGTH, reply, Config::MAX_MESSAGE_LENGTH);
    // uart_printf(CONSOLE, "[Client: %u ]: Successfully quit!\n\r", MyTid());
    // Exit();
}

class rps_player {
    rps_player* next = nullptr;
    uint32_t TID;
    rps_player* paired_with = nullptr;
    uint32_t slabIdx;

    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

   public:
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

void RPS_Server() {
    RegisterAs("rps_server");

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
        uint32_t clientTID;
        char msg[128];

        // uart_printf(CONSOLE, "waiting to receive \n\r");
        // once we go to receive the next task, there are no more tasks in the scheduler
        // issue: After receiving the first task, once we go back here to receive the second, scheduler goes to idle

        // uart_printf(CONSOLE, "RPS SERVER CALLED RECEIVE \n\r");
        int msgsize = Receive(&clientTID, msg, 128);

        uart_printf(CONSOLE, "[RPS Server]: Request from TID: %u: '%s' \n\r", clientTID, msg);
        // this will parse the message we receive
        //  split along the first space, if found
        char command[64];
        char param[64];
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
        for (int i = 0; i < 64; i++){
            uart_putc(CONSOLE, param[i]);
            if(param[i] == '\0'){
                uart_putc(CONSOLE, '!');
            }

        }
        uart_putc(CONSOLE, '\n');
        uart_putc(CONSOLE, '\r');
        // uart_printf(CONSOLE, "Param: %s, len: %d\n\r", param, strlen(param));
        // uart_printf(CONSOLE, "Param: %s, len: %d\n\r", param, strlen(param));



        // parse info
        if (!strcmp(command, "SIGNUP")) {  // if match, strcmp equals zero
            // once two are on queue, reply and ask for first play
            // player_queue.push(&client);
            if (freelist.empty()) {
                uart_printf(CONSOLE, "No more space in free list!!\n\r");
            }
            // get new rps player from slab
            rps_player* newplayer = freelist.pop();
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

            player_one->setPair(player_two);
            player_two->setPair(player_one);

            // push both of them on the map of pairs

        } else if (!strcmp(command, "PLAY")) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (playerSlabs[i].getTid() == clientTID) {
                    rps_player* partner = playerSlabs[i].getPair();
                    if (!partner) {
                        uart_printf(CONSOLE, "ERROR: Trying to play without signing up!\n\r");
                        break;
                    }

                    if (playerSlabs[i].quit) {
                        // partner sets this to quit upon them quitting
                        // If we put them back on the queue, our logic flow would get wayyy to complex
                        char msg[] = "Sorry, your partner quit.";
                        Reply(clientTID, msg, Config::MAX_MESSAGE_LENGTH);
                        freelist.push(&playerSlabs[i]);
                        break;
                    }

                    int clientAction = 0;
                    // int strval =  strcmp(param, "SCISSORS");
                    // uart_printf(CONSOLE, "FIRST GOT: %s strcmp: %d\n\r", param, strval);
                    if (!strcmp(param, "ROCK")) {
                        clientAction = 1;
                    } else if (!strcmp(param, "PAPER")) {
                        clientAction = 2;
                    } else if (!strcmp(param, "SCISSORS")) {
                        clientAction = 3;
                    } else {
                        uart_printf(CONSOLE,
                             "ERROR: NOT A VALID ACTION FROM {ROCK, PAPER, SCISSORS} FROM TID %u: \n\r", clientTID);
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

        } else if (!strcmp(command, "QUIT")) {
            // remove from our struct
            // uart_printf(CONSOLE, "Reached Quit \r\n");
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (playerSlabs[i].getTid() == clientTID) {
                    playerSlabs[i].getPair()->quit = 1;  // let our partner know we quit
                    freelist.push(&playerSlabs[i]);

                    char quit_msg[] = "You have quit";
                    Reply(clientTID, quit_msg, Config::MAX_MESSAGE_LENGTH);
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
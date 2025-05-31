#include "user_tasks.h"

#include <cstdint>
#include <string.h>

#include "queue.h"
#include "stack.h"
#include <map>
#include "fixed_map.h"
#include "rpi.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"


void FirstUserTask(){
    //  Assumes FirstUserTask is at Priority 2
    const uint64_t lowerPrio = 2 - 1;
    const uint64_t higherPrio = 2 + 1;

    // create two tasks at a lower priority
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));
    uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, &TestTask));

    // create two tasks at a higher priority
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

void RPS_Server();
void RPS_Client();

void RPSFirstUserTask(){

    uart_printf(CONSOLE, "[First User Task] Created NameServer: %u\n\r", Create(9, &NameServer));

    uart_printf(CONSOLE, "[First User Task] Created RPS Server: %u\n\r", Create(9, &RPS_Server));
   
    uart_printf(CONSOLE, "[First User Task] Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client)); 

    uart_printf(CONSOLE, "[First User Task] Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client)); 


    Exit();
}


void NameServer() {
    
    using Key = uint64_t;
    using Value = char*;
    using Pair = std::pair<const Key, Value>;
    // Choose a block size large enough for internal node type
    // MapCapacity similarly
    constexpr std::size_t MapCapacity = 128;
    constexpr std::size_t BLOCK_SIZE = 64;

    Pool<BLOCK_SIZE, MapCapacity> pool;
    PoolAllocator<Pair, BLOCK_SIZE, MapCapacity> alloc(&pool);

    std::map<Key, Value, std::less<Key>, PoolAllocator<Pair, BLOCK_SIZE, MapCapacity>> NameMap(std::less<Key>(), alloc);

    // Testing out map implementation
    uint64_t newTID = Create(1, &TestTask);
    char test[] = "AAAA";
    NameMap.emplace(newTID, test);

    uart_printf(CONSOLE, "Got value: %s\n\r", NameMap[newTID]);

    auto it = NameMap.find(1234);
    if(it == NameMap.end()){
         uart_printf(CONSOLE, "not found\n\r");
    }

    it = NameMap.find(newTID);
    if(it != NameMap.end()){
         uart_printf(CONSOLE, "found: %u, %s\n\r", it->first, it->second);
    }

    Exit();

}

void RPS_Client() {
    RegisterAs("rps_client");
    // find RPS server 
    int serverTID = WhoIs("rps_server");

    char msg[] = "SIGNUP";
    char reply[64];
    Send(serverTID, msg, strlen(msg), reply, 64);
    uart_printf(CONSOLE, "[Task: %u] Successfully signed up!", MyTid());

    //strcpy(msg, "PLAY ROCK");
    char msg2[] = "PLAY ROCK";
    Send(serverTID, msg2, strlen(msg), reply, 64);
    uart_printf(CONSOLE, "[Task: %u] Successfully played!", MyTid());

    //strcpy(msg, "PLAY ROCK");
    char msg3[] = "QUIT";
    Send(serverTID, msg3, strlen(msg), reply, 64);
    uart_printf(CONSOLE, "[Task: %u] Successfully quit!", MyTid());
    Exit();

}

class rps_player{
    rps_player* next = nullptr;
    uint64_t TID;
    rps_player*  paired_with = nullptr;
    uint64_t slabIdx;
    
    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

    public:
        // no oop bc lazy :(
        int waiting_on_pair = 0;
        int quit = 0;
        rps_player(uint64_t newTID=0){
            TID = newTID;
        }
        uint64_t getTid() {return TID;}
        rps_player*  getPair() {return paired_with;}
        void setTid(uint64_t id){ TID = id;}
        void setPair(rps_player*  id){ paired_with = id;}
};



void RPS_Server() {
    RegisterAs("rps_server");

    // signup queue, popped two at a time
    const int MAX_PLAYERS = 64;
    Queue<rps_player> player_queue;
    rps_player playerSlabs[MAX_PLAYERS]; 
    Stack<rps_player> freelist;

    //initialize our structs
    for (int i = 0; i < MAX_PLAYERS; i += 1) {
        freelist.push(&playerSlabs[i]);
    } 


    for(;;){
        uint64_t* clientTID = nullptr;
        char msg[128];
        int msgsize = Receive(clientTID, msg, 128);

        uart_printf(CONSOLE, "[RPS Server]: Request from TID: %u , with contents: %s \n\r", *clientTID, msg);

        //this will parse the message we receive
        // split along the first space, if found
        char command[64];
        char param[64];
        int msg_parse = 0;
        int tracker = 0;
        for(int i=0; i<msgsize; i++){
            if (!msg_parse){
                if(msg[i] != ' '){
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

        //parse info
        if (!strcmp(command, "SIGNUP")){ // if match, strcmp equals zero
            //once two are on queue, reply and ask for first play
            //player_queue.push(&client);
            if (freelist.empty()){
                uart_printf(CONSOLE, "No more space in free list\r\n");
            }
            // get new rps player from slab
            rps_player* newplayer = freelist.pop();
            newplayer->setTid(*clientTID);
            // put them on the end of the queue
            player_queue.push(newplayer);

            if(player_queue.tally() < 2){
                continue;
            }

            char signup_msg[] = "What is your play?";

            rps_player* player_one = player_queue.pop();
            Reply(player_one->getTid(), signup_msg, strlen(signup_msg)); 

            rps_player* player_two = player_queue.pop();
            Reply(player_two->getTid(), signup_msg, strlen(signup_msg));

            player_one->setPair(player_two);
            player_two->setPair(player_one);

            //push both of them on the map of pairs

        } else if (!strcmp(command, "PLAY")){

            for(int i=0; i<MAX_PLAYERS; i++){
                if (playerSlabs[i].getTid() == *clientTID){

                    rps_player* partner = playerSlabs[i].getPair();
                    if(!partner){
                        uart_printf(CONSOLE, "ERROR: Trying to play without signing up!");
                        break;
                    }

                    if(playerSlabs[i].quit){
                        // partner sets this to quit upon them quitting
                        // If we put them back on the queue, our logic flow would get wayyy to complex
                        char msg[] = "Sorry, your partner quit. Please signup again";
                        Reply(*clientTID, msg, strlen(msg));
                        freelist.push(&playerSlabs[i]);
                        break;
                    }


                    int clientAction = 0;
                    if (!strcmp(param, "ROCK")){
                        clientAction = 1;
                    }else if(!strcmp(param, "PAPER")) {
                        clientAction = 2;
                    } else if(!strcmp(param, "SCISSORS")) {
                        clientAction = 3;
                    } else {
                        uart_printf(CONSOLE, "ERROR: NOT A VALID ACTION FROM {ROCK, PAPER, SCISSORS}");
                        break;
                    }

                    if(!partner->waiting_on_pair){
                        //partner is not waiting on me (I have played first)
                        //so I am now waiting for my partner to send their message
                        playerSlabs[i].waiting_on_pair = clientAction;
                    } else {
                        //partner was waiting on me, time to do the game!
                        int partnerAction = partner->waiting_on_pair;
                        const char* winner_msg = "Winner!";
                        const char* loser_msg = "Loser!";
                        const char* tie_msg = "Tied!";

                        if(clientAction == partnerAction ){
                            //tied
                            Reply(*clientTID, tie_msg, strlen(loser_msg));
                            Reply(partner->getTid(), tie_msg, strlen(loser_msg));

                        } else if(clientAction == 1 && partnerAction == 2){
                            //client loss, partner won (rock loses to paper)
                            Reply(*clientTID, loser_msg, strlen(loser_msg));
                            Reply(partner->getTid(), winner_msg, strlen(loser_msg));

                        } else if(clientAction == 1 && partnerAction == 3){
                            //client won, partner loss (rock beats scissors)
                            Reply(*clientTID, winner_msg, strlen(loser_msg));
                            Reply(partner->getTid(), loser_msg, strlen(loser_msg));

                        } else if(clientAction == 2 && partnerAction == 1){
                            //client won, partner loss (paper beats rock)
                            Reply(*clientTID, winner_msg, strlen(loser_msg));
                            Reply(partner->getTid(), loser_msg, strlen(loser_msg));

                        } else if(clientAction == 2 && partnerAction == 3){
                            //client loss, partner won (paper loses to scissors)
                            Reply(*clientTID, loser_msg, strlen(loser_msg));
                            Reply(partner->getTid(), winner_msg, strlen(loser_msg));

                        } else if(clientAction == 3 && partnerAction == 1){
                            //client loss, partner won (scissors loses to rock)
                            Reply(*clientTID, loser_msg, strlen(loser_msg));
                            Reply(partner->getTid(), winner_msg, strlen(loser_msg));

                        } else if(clientAction == 3 && partnerAction == 2){
                            //client won, partner loss (scissors beats paper)
                            Reply(*clientTID, winner_msg, strlen(loser_msg));
                            Reply(partner->getTid(), loser_msg, strlen(loser_msg));
                        }
                        // clear their waiting 
                        playerSlabs[i].waiting_on_pair = 0;
                        partner->waiting_on_pair = 0;
                        
                    }
                    //found our desired slab, stop looping through the rest
                    break;
                }
            }


        } else if (!strcmp(command, "QUIT")){
            //remove from our struct
            for(int i=0; i<MAX_PLAYERS; i++){
                if (playerSlabs[i].getTid() == *clientTID){
                    playerSlabs[i].getPair()->quit = 1; // let our partner know we quit
                    freelist.push(&playerSlabs[i]);
                    break;
                }
            }
        } else {
            uart_printf(CONSOLE, "That was not a valid command! First word must be {SIGNUP, PLAY, QUIT}");
        }

    }


}
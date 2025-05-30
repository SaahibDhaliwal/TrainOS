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

// FirstUserTask FirstUserTask::instance;
// void FirstUserTask::main(){
//      // Assumes FirstUserTask is at Priority 2
//     const uint64_t lowerPrio = 2 - 1;
//     const uint64_t higherPrio = 2 + 1;

//     // create two tasks at a lower priority
//     uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, TestTask::staticMain));
//     uart_printf(CONSOLE, "Created: %u\n\r", Create(lowerPrio, TestTask::staticMain));

//     // create two tasks at a higher priority
//     uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, TestTask::staticMain));
//     uart_printf(CONSOLE, "Created: %u\n\r", Create(higherPrio, TestTask::staticMain));

//     uart_printf(CONSOLE, "FirstUserTask: exiting\n\r");
//     Exit();
// }

// TestTask TestTask::instance;
// void TestTask::main() {
//     uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
//     Yield();
//     uart_printf(CONSOLE, "[Task %u] Parent: %u\n\r", MyTid(), MyParentTid());
//     Exit();
// }

// IdleTask IdleTask::instance;
// void IdleTask::main() {
//     while (true) {
//         asm volatile("wfe");
//     }
// }

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


void RPSFirstUserTask(){

    uart_printf(CONSOLE, "Created NameServer: %u\n\r", Create(9, &NameServer));

    uart_printf(CONSOLE, "Created RPS Server: %u\n\r", Create(9, &RPS_Server));
   
    uart_printf(CONSOLE, "Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client)); 

    uart_printf(CONSOLE, "Created RPS Client with TID: %u\n\r", Create(9, &RPS_Client)); 


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
    char* test = "AAAA";
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
    uint64_t paired_with;

    uint64_t slabIdx;
    
    
    template <typename T>
    friend class Queue;

    template <typename T>
    friend class Stack;

    public:
        rps_player(uint64_t newTID=0){
            TID = newTID;
        }
        uint64_t getTid() {return TID;}
        void setTid(uint64_t id){ TID = id;}
        void setSlabIdx(uint64_t idx){
            slabIdx = idx;
        }
        void setPair(uint64_t id){ paired_with = id;}
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
        playerSlabs[i].setSlabIdx(i);
        freelist.push(&playerSlabs[i]);
    } 



    for(;;){
        uint64_t* clientTID = nullptr;
        char msg[128];
        int msgsize = Receive(clientTID, msg, 128);

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
            rps_player* newplayer = freelist.pop();
            newplayer->setTid(*clientTID);
            player_queue.push(newplayer);

            if(player_queue.tally() < 2){
                continue;
            } 

            rps_player* player_one = player_queue.pop();
            Reply(player_one->getTid(), "What is your play?", 19);

            rps_player* player_two = player_queue.pop();
            Reply(player_two->getTid(), "What is your play?", 19);

            //player_one->

            //push both of them on the map of pairs
            

        } else if (!strcmp(command, "PLAY")){
            //should check if they're already playing
            //DOESNT WORK ATM
            // if(*clientTID != player1.TID || *clientTID != player2.TID){
            //     //reply with error
            //     Reply(*clientTID, "Error: Did not register!", 21);
            // }
            

        } else if (!strcmp(command, "QUIT")){
            //remove from our struct
            //freelist.push()
            for(int i=0; i<MAX_PLAYERS; i++){
                if (playerSlabs[i].getTid() == *clientTID){
                    freelist.push(&playerSlabs[i]);
                }
            }
        }

    }


}
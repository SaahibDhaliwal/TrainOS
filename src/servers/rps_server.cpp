#include "servers/rps_server.h"

#include <string.h>

#include "config.h"
#include "generic_protocol.h"
#include "intrusive_node.h"
#include "name_server.h"
#include "ns_protocol.h"
#include "queue.h"
#include "rpi.h"
#include "rps_protocol.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "task_descriptor.h"
#include "user_tasks.h"
#include "util.h"

using namespace rps;

namespace {
class RPSPlayer : public IntrusiveNode<RPSPlayer> {
   public:
    uint32_t tid = 0;
    RPSPlayer* opponent = nullptr;
    Move move = Move::NONE;
    bool exited = false;

    void initialize(int newTid) {
        move = Move::NONE;
        exited = false;
        tid = newTid;
        opponent = nullptr;
    }
};
}  // namespace

void RPS_Server() {
    int registerReturn = name_server::RegisterAs(RPS_SERVER_NAME);
    if (registerReturn == -1) {
        uart_printf(CONSOLE, "UNABLE TO REACH NAME RPS Server \r\n");
        sys::Exit();
    }

    // signup queue, popped two at a time
    Queue<RPSPlayer> playerQueue;
    RPSPlayer playerSlabs[Config::MAX_TASKS];
    Stack<RPSPlayer> freelist;

    // initialize our structs
    for (int i = 0; i < Config::MAX_TASKS; i += 1) {
        freelist.push(&playerSlabs[i]);
    }

    for (;;) {
        uint32_t clientTid;
        char msg[3];
        int msgLen = sys::Receive(&clientTid, msg, 2);
        msg[msgLen] = '\0';

        Command command = commandFromByte(msg[0]);
        Move move = (command == Command::PLAY && msgLen >= 2) ? moveFromByte(msg[1]) : Move::NONE;

        uart_printf(CONSOLE, "[RPS Server]: Request from [Client %u]: %s", clientTid, commandToString(command));
        if (command == Command::PLAY) {
            uart_printf(CONSOLE, " %s", moveToString(move));
        }
        uart_printf(CONSOLE, "\r\n");

        // parse info
        switch (command) {
            case Command::SIGNUP: {
                // once two are on queue, reply and ask for first play
                if (freelist.empty()) {
                    uart_printf(CONSOLE, "No more space in free list!!\r\n");
                }
                // get new rps player from slab
                RPSPlayer* newPlayer = freelist.pop();
                newPlayer->initialize(clientTid);

                // put them on the end of the queue
                playerQueue.push(newPlayer);

                if (playerQueue.size() < 2) {
                    continue;
                }

                RPSPlayer* p1 = playerQueue.pop();
                sys::Reply(p1->tid, "", 0);
                RPSPlayer* p2 = playerQueue.pop();
                sys::Reply(p2->tid, "", 0);

                uart_printf(CONSOLE, "[RPS Server]: [Client %u] is paired with [Client %u]\r\n", p1->tid, p2->tid);

                p1->opponent = p2;
                p2->opponent = p1;

                break;
            }
            case Command::PLAY: {
                for (int i = 0; i < Config::MAX_TASKS; i++) {
                    if (playerSlabs[i].tid == clientTid) {
                        RPSPlayer* opponent = playerSlabs[i].opponent;
                        if (!opponent) {
                            uart_printf(CONSOLE,
                                        "[RPS Server]: ERROR! [Client %u] trying to play without signing up!\r\n",
                                        clientTid);
                            break;
                        }

                        if (playerSlabs[i].exited) {
                            // opponent sets this to quit upon them quitting
                            charReply(clientTid, static_cast<char>(Reply::OPPONENT_QUIT));
                            freelist.push(&playerSlabs[i]);
                            break;
                        }

                        if (move != Move::ROCK && move != Move::PAPER && move != Move::SCISSORS) {
                            uart_printf(CONSOLE, "[RPS Server]: UNKNOWN_MOVE from client %u\r\n", clientTid);
                            break;
                        }

                        if (opponent->move == Move::NONE) {
                            playerSlabs[i].move = move;
                        } else {
                            // opponent was waiting on me, time to do the game!
                            Move opponentMove = opponent->move;
                            const char winChar = static_cast<char>(Reply::WINNER);
                            const char loseChar = static_cast<char>(Reply::LOSER);
                            const char tieChar = static_cast<char>(Reply::TIED);

                            if (move == opponentMove) {
                                // tied
                                charReply(clientTid, tieChar);
                                charReply(opponent->tid, tieChar);
                            } else if (move == Move::ROCK && opponentMove == Move::PAPER) {
                                // client loss, opponent won (rock loses to paper)
                                charReply(clientTid, loseChar);
                                charReply(opponent->tid, winChar);
                            } else if (move == Move::ROCK && opponentMove == Move::SCISSORS) {
                                // client won, opponent loss (rock beats scissors)
                                charReply(clientTid, winChar);
                                charReply(opponent->tid, loseChar);

                            } else if (move == Move::PAPER && opponentMove == Move::ROCK) {
                                // client won, opponent loss (paper beats rock)
                                charReply(clientTid, winChar);
                                charReply(opponent->tid, loseChar);

                            } else if (move == Move::PAPER && opponentMove == Move::SCISSORS) {
                                // client loss, opponent won (paper loses to scissors)
                                charReply(clientTid, loseChar);
                                charReply(opponent->tid, winChar);

                            } else if (move == Move::SCISSORS && opponentMove == Move::ROCK) {
                                // client loss, opponent won (scissors loses to rock)
                                charReply(clientTid, loseChar);
                                charReply(opponent->tid, winChar);

                            } else if (move == Move::SCISSORS && opponentMove == Move::PAPER) {
                                // client won, opponent loss (scissors beats paper)
                                charReply(clientTid, winChar);
                                charReply(opponent->tid, loseChar);
                            }
                            // clear their waiting
                            playerSlabs[i].move = Move::NONE;
                            opponent->move = Move::NONE;
                        }
                        // found our desired slab, stop looping through the rest
                        break;
                    }
                }

                break;
            }
            case Command::QUIT: {
                // remove from our struct
                for (int i = 0; i < Config::MAX_TASKS; i++) {
                    if (playerSlabs[i].tid == clientTid) {
                        playerSlabs[i].opponent->exited = true;  // let our opponent know we quit CRUCIAL
                        freelist.push(&playerSlabs[i]);

                        charReply(clientTid, static_cast<char>(Reply::OPPONENT_QUIT));

                        // if our opponent was waiting for us, we need to free them
                        if (playerSlabs[i].opponent->move != Move::NONE) {
                            freelist.push(playerSlabs[i].opponent);
                            charReply(playerSlabs[i].opponent->tid, static_cast<char>(Reply::OPPONENT_QUIT));
                        }

                        break;
                    }
                }

                break;
            }
            default: {
                uart_printf(CONSOLE, "[RPS Server]: Unknown Command!\r\n");

                break;
            }
        }
    }
    sys::Exit();  // in case we ever somehow break out of the infinite for loop
}

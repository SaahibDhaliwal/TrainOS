#ifndef __RPS_PROTOCOL__
#define __RPS_PROTOCOL__

namespace rps {
enum class Command : char { SIGNUP, PLAY, QUIT, COUNT, UNKNOWN_COMMAND };

enum class Move : char { NONE, ROCK, PAPER, SCISSORS, COUNT, UNKNOWN_MOVE };

enum class Reply : char { OPPONENT_QUIT, WINNER, LOSER, TIED, FAIL, COUNT, UNKNOWN_REPLY };

char toByte(Command c);
char toByte(Move m);
char toByte(Reply r);

Command commandFromByte(char c);
Move moveFromByte(char c);
Reply replyFromByte(char c);

const char* commandToString(Command c);
const char* moveToString(Move m);
const char* replyToString(Reply r);

Reply Play(int serverTid, Move move);
int Signup(int serverTid);
int Quit(int serverTid);

}  // namespace rps
#endif
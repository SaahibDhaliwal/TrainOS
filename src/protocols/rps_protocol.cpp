#include "protocols/rps_protocol.h"

#include "sys_call_stubs.h"

namespace rps {

char toByte(Command c) {
    return static_cast<char>(c);
}

char toByte(Move m) {
    return static_cast<char>(m);
}

char toByte(Reply r) {
    return static_cast<char>(r);
}

Command commandFromByte(char c) {
    if (c < static_cast<char>(Command::COUNT)) {
        return static_cast<Command>(c);
    }

    return Command::UNKNOWN_COMMAND;
}

Move moveFromByte(char c) {
    if (c < static_cast<char>(Move::COUNT)) {
        return static_cast<Move>(c);
    }

    return Move::UNKNOWN_MOVE;
}

Reply replyFromByte(char c) {
    if (c < static_cast<char>(Reply::COUNT)) {
        return static_cast<Reply>(c);
    }

    return Reply::UNKNOWN_REPLY;
}

const char* commandToString(Command c) {
    switch (c) {
        case Command::SIGNUP:
            return "SIGNUP";
        case Command::PLAY:
            return "PLAY";
        case Command::QUIT:
            return "QUIT";
        case Command::UNKNOWN_COMMAND:
            return "UNKNOWN COMMAND";
        default:
            return "INVALID COMMAND";
    }
}

const char* moveToString(Move m) {
    switch (m) {
        case Move::NONE:
            return "NONE";
        case Move::ROCK:
            return "ROCK";
        case Move::PAPER:
            return "PAPER";
        case Move::SCISSORS:
            return "SCISSORS";
        case Move::UNKNOWN_MOVE:
            return "UNKNOWN MOVE";
        default:
            return "INVALID MOVE";
    }
}

const char* replyToString(Reply r) {
    switch (r) {
        case Reply::OPPONENT_QUIT:
            return "OPPONENT QUIT";
        case Reply::WINNER:
            return "WINNER";
        case Reply::LOSER:
            return "LOSER";
        case Reply::TIED:
            return "TIED";
        case Reply::FAIL:
            return "FAIL";
        case Reply::UNKNOWN_REPLY:
            return "UNKNOWN REPLY";
        default:
            return "INVALID REPLY";
    }
}

Reply Play(int serverTid, Move move) {
    char msg[3] = {0};
    msg[0] = toByte(Command::PLAY);
    msg[1] = toByte(move);

    char reply[1] = {0};
    int result = sys::Send(serverTid, msg, 2, reply, 1);

    if (result < 0) return Reply::FAIL;

    return replyFromByte(reply[0]);
}

int Signup(int serverTid) {
    char msg[2] = {0};
    msg[0] = toByte(Command::SIGNUP);

    int result = sys::Send(serverTid, msg, 1, nullptr, 0);
    if (result < 0) return -1;

    return 0;
}

int Quit(int serverTid) {
    char msg[2] = {0};
    msg[0] = toByte(Command::QUIT);

    int result = sys::Send(serverTid, msg, 2, nullptr, 0);
    if (result < 0) return -1;

    return 0;
}
}  // namespace rps

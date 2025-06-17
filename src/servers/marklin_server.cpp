

#include "config.h"
// #include "cs_protocol.h"
// #include "generic_protocol.h"
// #include "idle_time.h"
// #include "interrupt.h"
// #include "intrusive_node.h"
// #include "name_server.h"
#include "co_protocol.h"
// #include "queue.h"
// #include "rpi.h"
// #include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

using namespace console_server;

typedef enum {
    GOOD_TO_TRANSMIT,
    WAITING_ON_BOX,
    SLOW_BOX,  // Tx high, CTS high, but CTS did not go low
    BOTH_SLOW,
    BOX_PROCESSING,
} state_t;

class StateMachine {
   private:
    /* data */
   public:
    StateMachine(/* args */);
};

StateMachine::StateMachine(/* args */) {
}

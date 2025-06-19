#ifndef __INTERRUPT__
#define __INTERRUPT__

#include <cstdint>

enum class INTERRUPT_NUM { CLOCK = 97, UART = 153, COUNT };
enum class EVENT_ID { CLOCK, CONSOLE_ALL, CONSOLE_TX, CONSOLE_RXRT, BOX, COUNT };  // these might have a better name

#endif /* interrupt.h */
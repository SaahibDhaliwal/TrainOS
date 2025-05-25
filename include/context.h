#ifndef __CONTEXT__
#define __CONTEXT__
#include <cstdint>

struct __attribute__((packed)) Context {
    int64_t registers[31];
    uint64_t sp;
    uint64_t elr;
    uint64_t spsr;
};

#endif /* context.h */

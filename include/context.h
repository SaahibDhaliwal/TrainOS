#ifndef __CONTEXT__
#define __CONTEXT__
#include <stdint.h>

struct __attribute__((packed)) Context {
    int64_t registers[31];
    uint64_t sp;
    uint64_t elr;
    uint64_t spsr;
};

extern "C" {
uint32_t kernelToUser(Context* kernelContext, Context* userTaskContext);
uint32_t userToKernel(Context* kernelContext, Context* userTaskContext);
uint32_t slowKernelToUser(Context* kernelContext, Context* userTaskContext);
uint32_t slowUserToKernel(Context* kernelContext, Context* userTaskContext);
}

#endif /* context.h */

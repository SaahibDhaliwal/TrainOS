#ifndef _taskcreation_h
#define _taskcreation_h_ 1

// Allocates and initializes a task descriptor, using the given priority,
// and the given function pointer as a pointer to the entry point of executable code.

//not sure what this is used for atm -Dan
class TaskDescriptor {
      public:
    TaskDescriptor() {};
};

// When Create returns, the task descriptor has all the state needed to run the task, the task's stack has been suitably
// initialized, and the task has been entered into its ready queue so that it will run the next time it is scheduled.
extern int Create(int priority, void (*function)());

// Returns the task id of the calling task.
extern int MyTid();

// Returns the task id of the task that created the calling task
extern int MyParentTid();

// Causes a task to pause executing. The task is moved to the end of its priority queue, and will resume executing when
// next scheduled.
extern void Yield();

// Causes a task to cease execution permanently. It is removed from all priority queues, send queues, receive queues and
// event queues Resources owned by the task, primarily its memory and task descriptor, may be reclaimed.
extern void Exit();


void FirstUserTask();

void TestTask();

extern uint64_t getRegisterx1(void);
extern uint64_t getRegisterx2(void);
extern uint64_t getRegisterx3(void);
extern uint64_t getRegisterx4(void);
extern uint64_t getRegisterx5(void);
extern uint64_t getRegisterx6(void);
extern uint64_t getRegisterx7(void);
extern uint64_t getRegisterx8(void);
extern uint64_t getRegisterx9(void);
extern uint64_t getRegisterx10(void);
extern uint64_t getRegisterx11(void);
extern uint64_t getRegisterx12(void);
extern uint64_t getRegisterx13(void);
extern uint64_t getRegisterx14(void);
extern uint64_t getRegisterx15(void);
extern uint64_t getRegisterx16(void);
extern uint64_t getRegisterx17(void);
extern uint64_t getRegisterx18(void);
extern uint64_t getRegisterx19(void);
extern uint64_t getRegisterx20(void);
extern uint64_t getRegisterx21(void);
extern uint64_t getRegisterx22(void);
extern uint64_t getRegisterx23(void);
extern uint64_t getRegisterx24(void);
extern uint64_t getRegisterx25(void);
extern uint64_t getRegisterx26(void);
extern uint64_t getRegisterx27(void);
extern uint64_t getRegisterx28(void);
extern uint64_t getRegisterx29(void);
extern uint64_t getRegisterx30(void);

extern void setRegisterx1(uint64_t value);
extern void setRegisterx2(uint64_t value);
extern void setRegisterx3(uint64_t value);
extern void setRegisterx4(uint64_t value);
extern void setRegisterx5(uint64_t value);
extern void setRegisterx6(uint64_t value);
extern void setRegisterx7(uint64_t value);
extern void setRegisterx8(uint64_t value);
extern void setRegisterx9(uint64_t value);
extern void setRegisterx10(uint64_t value);
extern void setRegisterx11(uint64_t value);
extern void setRegisterx12(uint64_t value);
extern void setRegisterx13(uint64_t value);
extern void setRegisterx14(uint64_t value);
extern void setRegisterx15(uint64_t value);
extern void setRegisterx16(uint64_t value);
extern void setRegisterx17(uint64_t value);
extern void setRegisterx18(uint64_t value);
extern void setRegisterx19(uint64_t value);
extern void setRegisterx20(uint64_t value);
extern void setRegisterx21(uint64_t value);
extern void setRegisterx22(uint64_t value);
extern void setRegisterx23(uint64_t value);
extern void setRegisterx24(uint64_t value);
extern void setRegisterx25(uint64_t value);
extern void setRegisterx26(uint64_t value);
extern void setRegisterx27(uint64_t value);
extern void setRegisterx28(uint64_t value);
extern void setRegisterx29(uint64_t value);
extern void setRegisterx30(uint64_t value);
#endif /* taskcreation.h */
#ifndef __USER_TASKS__
#define __USER_TASKS__

#include "task_descriptor.h"

// class BaseTask {
//     public:
//         virtual ~BaseTask() = default;
//         virtual void main();
// };

// class FirstUserTask: public BaseTask {
//     private:
//         static FirstUserTask instance;
//     public:
//         void main() override;
//         static void staticMain(){
//             instance.main();
//         }
// };

// class TestTask: public BaseTask {
//     private:
//         static TestTask instance;
//     public:
//         void main() override;
//         static void staticMain(){
//             instance.main();
//         }
// };

// class IdleTask: public BaseTask {
//     private:
//         static IdleTask instance;
//     public:
//         void main() override;
//         static void staticMain(){
//             instance.main();
//         }
// };
void FirstUserTask();

void TestTask();

void IdleTask();

void NameServer();

void RPSFirstUserTask();

#endif /* user_tasks.h */
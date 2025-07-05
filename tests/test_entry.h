#include "containers/priority_queue_test.h"
#include "context/context_switch_test.h"
#include "test_utils.h"

void runTests() {
    // RUN_TEST(runContextSwitchTest);
    RUN_TEST(runPriorityQueueTest);

    while (true) {
        __asm__ volatile("wfe");
    }
}
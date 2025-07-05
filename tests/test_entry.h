#include "containers/priority_queue_test.h"
#include "containers/unordered_map_test.h"
#include "context/context_switch_test.h"
#include "test_utils.h"

void runTests() {
    // RUN_TEST(runContextSwitchTest);
    // RUN_TEST(runPriorityQueueTest);
    RUN_TEST(runUnorderedMapTest);

    while (true) {
        __asm__ volatile("wfe");
    }
}
#include "containers/priority_queue_test.h"
#include "containers/unordered_map_test.h"
#include "context/context_switch_test.h"
#include "pathfinding_test.h"
#include "test_utils.h"

void runTests() {
    // RUN_TEST(runPriorityQueueTest);
    // RUN_TEST(runUnorderedMapTest);
    RUN_TEST(runPathfindingTest);
    // int testIterations = 100;
    // for (int i = 0; i < testIterations; i++) {
    //     RUN_TEST(runContextSwitchTest);
    // }

    while (true) {
        __asm__ volatile("wfe");
    }
}
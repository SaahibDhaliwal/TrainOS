#include "containers/priority_queue_test.h"
#include "containers/unordered_map_test.h"
#include "context/context_switch_test.h"
#include "pathfinding_test.h"
#include "test_utils.h"

void runTests() {
    // RUN_TEST(runContextSwitchTest);
    // RUN_TEST(runPriorityQueueTest);
    RUN_TEST(runUnorderedMapTest);
    // RUN_TEST(runPathfindingTest);

    while (true) {
        __asm__ volatile("wfe");
    }
}
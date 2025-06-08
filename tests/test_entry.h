#include "context/context_switch_test.h"
#include "test_utils.h"

void runTests() {
    RUN_TEST(runContextSwitchTest);

    while (true) {
        __asm__ volatile("wfe");
    }
}